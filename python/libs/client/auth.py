#-------------------------------------------------------------------------------
# Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
#-------------------------------------------------------------------------------
# This file is part of the XRootD software suite.
#
# XRootD is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# XRootD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
#-------------------------------------------------------------------------------
from __future__ import absolute_import, division, print_function

import os
import tempfile
import uuid

from urllib.parse import quote, urlsplit, urlunsplit


def _credential_path(path):
  path = os.fsdecode(os.fspath(path))
  path = os.path.expandvars(os.path.expanduser(path))
  if not path:
    raise ValueError('credential paths must not be empty')
  if any(separator in path for separator in ('&', '?', '#')):
    raise ValueError('credential paths cannot contain URL query separators')
  return path


def _existing_path(path, directory=False):
  if not path:
    return None
  path = os.path.expandvars(os.path.expanduser(path))
  predicate = os.path.isdir if directory else os.path.isfile
  return path if predicate(path) else None


class AuthContext(object):
  """Object-scoped authentication configuration for XRootD clients.

  Authentication is attached to individual URLs and channels instead of being
  written to the process-wide XRootD environment.  Contexts are safe to share
  between threads.  Distinct contexts always use distinct client channels,
  even when they refer to the same endpoint and credential path.

  Create contexts with :meth:`bearer` or :meth:`x509`.
  """

  _ROOT_SCHEMES = frozenset(('root', 'roots', 'xroot', 'xroots'))
  _HTTP_SCHEMES = frozenset(('http', 'https', 'dav', 'davs'))

  def __init__(self, protocol, parameters, token_handle=None):
    self.__protocol = protocol
    self.__parameters = tuple(parameters)
    self.__token_handle = token_handle
    self.__identity = uuid.uuid4().hex
    self.__closed = False

  @classmethod
  def bearer(cls, token=None, token_file=None, ca_file=None, ca_dir=None,
             verify=True):
    """Create a bearer-token authentication context.

    Exactly one of ``token`` or ``token_file`` must be supplied.  A token
    supplied by value is stored in a private temporary file for the lifetime of
    the context because the ZTN security plug-in consumes a credential file.
    """
    if (token is None) == (token_file is None):
      raise ValueError('exactly one of token or token_file is required')

    token_handle = None
    if token is not None:
      if not isinstance(token, str):
        raise TypeError('token must be a string')
      if not token.strip():
        raise ValueError('token must not be empty')
      token_handle = tempfile.NamedTemporaryFile(
        mode='w', prefix='xrootd-token-', delete=True)
      token_handle.write(token)
      token_handle.flush()
      token_file = token_handle.name
    else:
      token_file = _credential_path(token_file)

    parameters = [('xrd.ztn', token_file),
                  ('xrdcl.http.bearertokenfile', token_file)]
    cls._add_tls_parameters(parameters, ca_file, ca_dir, verify)
    return cls('ztn', parameters, token_handle)

  @classmethod
  def x509(cls, proxy=None, cert=None, key=None, ca_file=None, ca_dir=None,
           verify=True):
    """Create an X.509 authentication context.

    Supply either a proxy path or both a certificate and private-key path.
    """
    has_proxy = proxy is not None
    has_cert_key = cert is not None or key is not None
    if has_proxy == has_cert_key:
      raise ValueError('supply either proxy or both cert and key')
    if has_cert_key and (cert is None or key is None):
      raise ValueError('cert and key must be supplied together')

    if has_proxy:
      proxy = _credential_path(proxy)
      parameters = [('xrd.gsiusrpxy', proxy),
                    ('xrdcl.http.clientcert', proxy),
                    ('xrdcl.http.clientkey', proxy)]
    else:
      cert = _credential_path(cert)
      key = _credential_path(key)
      parameters = [
        ('xrd.gsiusrcrt', _credential_path(cert)),
        ('xrd.gsiusrkey', _credential_path(key)),
        ('xrdcl.http.clientcert', cert),
        ('xrdcl.http.clientkey', key),
      ]
    cls._add_tls_parameters(parameters, ca_file, ca_dir, verify)
    return cls('gsi', parameters)

  @classmethod
  def anonymous(cls, ca_file=None, ca_dir=None, verify=True):
    """Disable ambient credentials for public or presigned HTTP URLs."""
    parameters = [('xrdcl.http.noauth', '1')]
    cls._add_tls_parameters(parameters, ca_file, ca_dir, verify)
    return cls(None, parameters)

  @classmethod
  def no_credentials(cls, ca_file=None, ca_dir=None, verify=True):
    """Create a context which never selects ambient credentials.

    HTTP URLs are explicitly anonymous. XRootD URLs request no security
    protocol, allowing public endpoints while failing against endpoints which
    require authentication.
    """
    parameters = [('xrdcl.http.noauth', '1')]
    cls._add_tls_parameters(parameters, ca_file, ca_dir, verify)
    return cls('none', parameters)

  @classmethod
  def from_environment(cls, token=None, token_file=None, proxy=None,
                       cert=None, key=None, ca_file=None, ca_dir=None,
                       verify=True, fallback='none', environ=None,
                       use_bearer_environment=True, use_defaults=True):
    """Snapshot credentials from explicit values and the environment.

    Explicit bearer or X.509 arguments take precedence. Standard bearer,
    XRootD GSI, X.509, and TLS environment variables are then considered,
    followed by conventional proxy and ``~/.globus`` paths. No process-global
    setting is modified.

    :param fallback: ``'none'`` to suppress all ambient authentication,
                     ``'anonymous'`` for HTTP-only anonymous access, or
                     ``'error'`` to reject a missing credential
    :param environ: optional environment mapping, primarily for applications
                    which need a deterministic snapshot
    """
    environ = os.environ if environ is None else environ

    if ca_file is None:
      ca_file = _existing_path(
        environ.get('X509_CERT_FILE') or environ.get('SSL_CERT_FILE'))
    if ca_dir is None:
      ca_dir = _existing_path(
        environ.get('X509_CERT_DIR') or environ.get('SSL_CERT_DIR'),
        directory=True)
    tls = {'ca_file': ca_file, 'ca_dir': ca_dir, 'verify': verify}

    if token is not None or token_file is not None:
      return cls.bearer(token=token, token_file=token_file, **tls)
    if use_bearer_environment:
      if 'BEARER_TOKEN_FILE' in environ:
        return cls.bearer(token_file=environ['BEARER_TOKEN_FILE'], **tls)
      if 'BEARER_TOKEN' in environ:
        return cls.bearer(token=environ['BEARER_TOKEN'], **tls)

    if proxy is not None:
      return cls.x509(proxy=proxy, **tls)
    if cert is not None or key is not None:
      return cls.x509(cert=cert, key=key, **tls)

    for variable in ('XrdSecGSIUSERPROXY', 'X509_USER_PROXY'):
      if variable in environ:
        return cls.x509(proxy=environ[variable], **tls)

    cert_variable = next((variable for variable in (
      'XrdSecGSIUSERCERT', 'X509_USER_CERT') if variable in environ), None)
    key_variable = next((variable for variable in (
      'XrdSecGSIUSERKEY', 'X509_USER_KEY') if variable in environ), None)
    if cert_variable is not None or key_variable is not None:
      return cls.x509(
        cert=environ.get(cert_variable) if cert_variable else None,
        key=environ.get(key_variable) if key_variable else None,
        **tls)

    if use_defaults and hasattr(os, 'geteuid'):
      default_proxy = _existing_path('/tmp/x509up_u%d' % os.geteuid())
      if default_proxy is not None:
        return cls.x509(proxy=default_proxy, **tls)
    if use_defaults:
      default_cert = _existing_path(os.path.join(
        os.path.expanduser('~'), '.globus', 'usercert.pem'))
      default_key = _existing_path(os.path.join(
        os.path.expanduser('~'), '.globus', 'userkey.pem'))
      if default_cert is not None and default_key is not None:
        return cls.x509(cert=default_cert, key=default_key, **tls)

    if fallback == 'none':
      return cls.no_credentials(**tls)
    if fallback == 'anonymous':
      return cls.anonymous(**tls)
    if fallback == 'error':
      raise ValueError('no usable authentication credential was found')
    raise ValueError("fallback must be 'none', 'anonymous', or 'error'")

  @staticmethod
  def _add_tls_parameters(parameters, ca_file, ca_dir, verify):
    if ca_file is not None:
      parameters.append(('xrdcl.http.cafile', _credential_path(ca_file)))
    if ca_dir is not None:
      parameters.append(('xrdcl.http.cadir', _credential_path(ca_dir)))
    if not verify:
      parameters.append(('xrdcl.http.noverify', '1'))

  def apply(self, url):
    """Return ``url`` with this context's client authentication parameters."""
    if self.__closed:
      raise RuntimeError('authentication context is closed')

    url = str(url)
    parsed = urlsplit(url)
    if parsed.scheme not in self._ROOT_SCHEMES | self._HTTP_SCHEMES:
      return url

    is_root = parsed.scheme in self._ROOT_SCHEMES
    parameters_for_scheme = [
      (key, value) for key, value in self.__parameters
      if (is_root and not key.startswith('xrdcl.http.')) or
         (not is_root and key.startswith('xrdcl.http.'))
    ]
    owned_keys = set(key for key, _ in parameters_for_scheme)
    owned_keys.add('xrdcl.authctx')
    if is_root:
      owned_keys.add('xrd.wantprot')
    parameters = [
      parameter
      for parameter in parsed.query.split('&')
      if parameter and parameter.split('=', 1)[0] not in owned_keys
    ]
    if is_root:
      if self.__protocol is None:
        raise ValueError('anonymous authentication is only supported for HTTP URLs')
      parameters.append('xrd.wantprot={}'.format(self.__protocol))
    parameters.extend(
      '{}={}'.format(key, quote(value, safe='/'))
      for key, value in parameters_for_scheme)
    parameters.append('xrdcl.authctx={}'.format(self.__identity))
    query = '&'.join(parameters)
    return urlunsplit(parsed._replace(query=query))

  def close(self):
    """Release any temporary credential owned by this context."""
    if self.__closed:
      return
    self.__closed = True
    if self.__token_handle is not None:
      self.__token_handle.close()

  def __enter__(self):
    if self.__closed:
      raise RuntimeError('authentication context is closed')
    return self

  def __exit__(self, type, value, traceback):
    self.close()

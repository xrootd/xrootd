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
  if any(separator in path for separator in ('&', '?', '#')):
    raise ValueError('credential paths cannot contain URL query separators')
  return path


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

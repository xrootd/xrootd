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

from urllib.parse import urlsplit, urlunsplit


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

  def __init__(self, protocol, parameters, token_handle=None):
    self.__protocol = protocol
    self.__parameters = tuple(parameters)
    self.__token_handle = token_handle
    self.__identity = uuid.uuid4().hex
    self.__closed = False

  @classmethod
  def bearer(cls, token=None, token_file=None):
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

    return cls('ztn', (('xrd.ztn', token_file),), token_handle)

  @classmethod
  def x509(cls, proxy=None, cert=None, key=None):
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
      parameters = (('xrd.gsiusrpxy', _credential_path(proxy)),)
    else:
      parameters = (
        ('xrd.gsiusrcrt', _credential_path(cert)),
        ('xrd.gsiusrkey', _credential_path(key)),
      )
    return cls('gsi', parameters)

  def apply(self, url):
    """Return ``url`` with this context's client authentication parameters."""
    if self.__closed:
      raise RuntimeError('authentication context is closed')

    url = str(url)
    parsed = urlsplit(url)
    if parsed.scheme not in self._ROOT_SCHEMES:
      return url

    owned_keys = set(key for key, _ in self.__parameters)
    owned_keys.update(('xrd.wantprot', 'xrdcl.authctx'))
    parameters = [
      parameter
      for parameter in parsed.query.split('&')
      if parameter and parameter.split('=', 1)[0] not in owned_keys
    ]
    parameters.append('xrd.wantprot={}'.format(self.__protocol))
    parameters.extend(
      '{}={}'.format(key, value) for key, value in self.__parameters)
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

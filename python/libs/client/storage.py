#-------------------------------------------------------------------------------
# Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
#-------------------------------------------------------------------------------
"""Small, transport-neutral storage operations for application clients."""

from __future__ import absolute_import, division, print_function

import math
import os
import posixpath
import time

from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

from XRootD.client.auth import AuthContext
from XRootD.client.copyprocess import CopyProcess
from XRootD.client.filesystem import FileSystem
from XRootD.client.flags import MkDirFlags, QueryCode
from XRootD.client.responses import XRootDAlreadyExistsError, XRootDNotFoundError


class StorageClient(object):
  """Convenient high-level operations over XRootD, HTTP, and WebDAV.

  The client owns no process-global state and is safe to share between threads.
  Authentication and the default deadline are configured once, then applied to
  every operation. Methods raise the mapped exceptions from
  :mod:`XRootD.client.responses` on failure.

  :param auth: object-scoped credentials, including ``AuthContext.anonymous()``
               for public or presigned HTTP URLs
  :param timeout: default end-to-end operation deadline in seconds
  """

  def __init__(self, auth=None, timeout=300):
    if auth is not None and not isinstance(auth, AuthContext):
      raise TypeError('auth must be an AuthContext')
    if timeout is not None and timeout <= 0:
      raise ValueError('timeout must be positive or None')
    self.__auth = auth
    self.__timeout = timeout

  def _deadline(self, timeout):
    timeout = self.__timeout if timeout is None else timeout
    if timeout is None:
      return None
    if timeout <= 0:
      raise ValueError('timeout must be positive')
    return time.monotonic() + timeout

  @staticmethod
  def _remaining(deadline):
    if deadline is None:
      return 0
    remaining = deadline - time.monotonic()
    if remaining <= 0:
      raise TimeoutError('storage operation deadline expired')
    return max(1, int(math.ceil(remaining)))

  def _filesystem(self, url):
    parsed = urlsplit(str(url))
    if not parsed.scheme or not parsed.netloc:
      raise ValueError('a full remote URL is required')
    endpoint = urlunsplit((parsed.scheme, parsed.netloc, '/', '', ''))
    filesystem = FileSystem(endpoint, auth=self.__auth)
    if parsed.query:
      filesystem.set_property('XrdClHttpQueryParam', parsed.query)
    return filesystem, parsed.path or '/'

  @staticmethod
  def _local_url(path):
    return Path(os.path.abspath(os.fspath(path))).as_uri()

  def _copy(self, source, target, deadline, force=False, mkdir=False):
    timeout = self._remaining(deadline)
    process = CopyProcess()
    process.add_job(source, target, force=force, mkdir=mkdir,
                    inittimeout=timeout, cptimeout=timeout,
                    source_auth=self.__auth, target_auth=self.__auth)
    process.prepare().raise_on_error()
    status, results = process.run()
    status.raise_on_error()
    if not results:
      raise RuntimeError('copy process returned no job result')
    results[0]['status'].raise_on_error()
    return results[0]

  def get(self, source, destination, timeout=None, force=False):
    """Download one URL to a local path."""
    return self._copy(source, self._local_url(destination),
                      self._deadline(timeout), force=force)

  def put(self, source, destination, timeout=None, force=False,
          create_parents=True):
    """Upload one local path, optionally creating destination parents."""
    deadline = self._deadline(timeout)
    parsed = urlsplit(str(destination))
    if create_parents and not parsed.query:
      parent = posixpath.dirname(parsed.path)
      if parent and parent != '/':
        parent_url = urlunsplit(parsed._replace(
          path=parent, query='', fragment=''))
        self.mkdir_p(parent_url, timeout=self._remaining(deadline))
    return self._copy(self._local_url(source), destination, deadline,
                      force=force)

  def delete(self, url, timeout=None):
    """Delete a file or collection; partial WebDAV deletes raise an error."""
    filesystem, path = self._filesystem(url)
    status, _ = filesystem.rm(path, timeout=self._remaining(
      self._deadline(timeout)))
    status.raise_on_error()

  def move(self, source, destination, timeout=None, create_parents=True):
    """Atomically rename a resource within one storage endpoint."""
    source_parsed = urlsplit(str(source))
    destination_parsed = urlsplit(str(destination))
    if (source_parsed.scheme, source_parsed.netloc) != (
        destination_parsed.scheme, destination_parsed.netloc):
      raise ValueError('move source and destination must share an endpoint')
    if source_parsed.query or destination_parsed.query:
      raise ValueError('move does not accept URL query parameters')
    deadline = self._deadline(timeout)
    filesystem, source_path = self._filesystem(source)

    def perform_move():
      status, _ = filesystem.mv(source_path, destination_parsed.path,
                                timeout=self._remaining(deadline))
      status.raise_on_error()

    try:
      perform_move()
    except XRootDAlreadyExistsError:
      parent = posixpath.dirname(destination_parsed.path)
      if not create_parents or not parent or parent == '/':
        raise
      parent_url = urlunsplit(destination_parsed._replace(
        path=parent, query='', fragment=''))
      self.mkdir_p(parent_url, timeout=self._remaining(deadline))
      perform_move()

  def stat(self, url, timeout=None):
    """Return metadata for a remote resource."""
    filesystem, path = self._filesystem(url)
    status, info = filesystem.stat(path, timeout=self._remaining(
      self._deadline(timeout)))
    status.raise_on_error()
    return info

  def exists(self, url, timeout=None):
    """Return whether a remote resource exists."""
    try:
      self.stat(url, timeout=timeout)
      return True
    except XRootDNotFoundError:
      return False

  def listdir(self, url, timeout=None):
    """Return a remote directory listing with entry metadata."""
    filesystem, path = self._filesystem(url)
    status, entries = filesystem.dirlist(path, timeout=self._remaining(
      self._deadline(timeout)))
    status.raise_on_error()
    return entries

  def mkdir_p(self, url, timeout=None):
    """Create a remote directory and all missing parents."""
    filesystem, path = self._filesystem(url)
    status, _ = filesystem.mkdir(path, flags=MkDirFlags.MAKEPATH,
                                 timeout=self._remaining(
                                   self._deadline(timeout)))
    status.raise_on_error()

  def space(self, url, timeout=None):
    """Return ``total``, ``free``, and ``used`` byte counts for an endpoint."""
    filesystem, path = self._filesystem(url)
    status, response = filesystem.query(QueryCode.SPACE, path,
                                        timeout=self._remaining(
                                          self._deadline(timeout)))
    status.raise_on_error()
    values = {}
    if isinstance(response, bytes):
      response = response.decode('ascii')
    for parameter in str(response).split('&'):
      key, separator, value = parameter.partition('=')
      if separator:
        values[key] = int(value)
    return {
      'total': values['oss.space'],
      'free': values['oss.free'],
      'used': values['oss.used'],
    }

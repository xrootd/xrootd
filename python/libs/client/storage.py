#-------------------------------------------------------------------------------
# Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
#-------------------------------------------------------------------------------
"""Small, transport-neutral storage operations for application clients."""

from __future__ import absolute_import, division, print_function

import math
import os
import posixpath
import threading
import time

from contextlib import contextmanager
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

from XRootD.client.auth import AuthContext
from XRootD.client.copyprocess import CopyProcess
from XRootD.client.filesystem import FileSystem
from XRootD.client.flags import QueryCode
from XRootD.client.responses import (XRootDAlreadyExistsError,
                                    XRootDChecksumError,
                                    XRootDNotFoundError,
                                    XRootDOperationError,
                                    XRootDUnsupportedError,
                                    XRootDTimeoutError)


STORAGE_CLIENT_API_VERSION = 2


class StorageInfo(object):
  """Transport-neutral metadata returned by :class:`StorageClient`.

  :var size: resource size in bytes
  :var checksum: optional structured checksum response
  :var stat: original native stat response
  :var checksum_errors: errors encountered while negotiating a checksum
  """

  def __init__(self, stat, checksum=None, checksum_errors=()):
    self.stat = stat
    self.size = stat['size'] if isinstance(stat, dict) else stat.size
    self.checksum = checksum
    self.checksum_errors = tuple(checksum_errors)


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

  def __init__(self, auth=None, timeout=300, owns_auth=False):
    if auth is not None and not isinstance(auth, AuthContext):
      raise TypeError('auth must be an AuthContext')
    if timeout is not None and timeout <= 0:
      raise ValueError('timeout must be positive or None')
    if not isinstance(owns_auth, bool):
      raise TypeError('owns_auth must be a boolean')
    self.__auth = auth
    self.__timeout = timeout
    self.__owns_auth = owns_auth
    self.__condition = threading.Condition()
    self.__active_operations = 0
    self.__closed = False
    self.__checksum_algorithms = {}
    self.__checksum_lock = threading.Lock()

  @classmethod
  def from_environment(cls, timeout=300, **auth_options):
    """Create a client which owns an environment-snapshot auth context."""
    auth = AuthContext.from_environment(**auth_options)
    try:
      return cls(auth=auth, timeout=timeout, owns_auth=True)
    except Exception:
      auth.close()
      raise

  @contextmanager
  def _operation(self):
    with self.__condition:
      if self.__closed:
        raise RuntimeError('storage client is closed')
      self.__active_operations += 1
    try:
      yield
    finally:
      with self.__condition:
        self.__active_operations -= 1
        if not self.__active_operations:
          self.__condition.notify_all()

  def close(self):
    """Wait for active operations and release an owned auth context."""
    with self.__condition:
      if self.__closed:
        return
      self.__closed = True
      while self.__active_operations:
        self.__condition.wait()
    if self.__owns_auth and self.__auth is not None:
      self.__auth.close()

  def __enter__(self):
    with self.__condition:
      if self.__closed:
        raise RuntimeError('storage client is closed')
    return self

  def __exit__(self, type, value, traceback):
    self.close()

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
      raise XRootDTimeoutError('storage operation deadline expired')
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

  def _stat(self, url, deadline):
    filesystem, path = self._filesystem(url)
    status, response = filesystem.stat(
      path, timeout=self._remaining(deadline))
    status.raise_on_error()
    return response

  def _checksum(self, url, algorithm, deadline):
    filesystem, path = self._filesystem(url)
    if algorithm:
      separator = '&' if '?' in path else '?'
      path = '{}{}cks.type={}'.format(path, separator, algorithm)
    status, response = filesystem.checksum(
      path, timeout=self._remaining(deadline))
    status.raise_on_error()
    return response

  def _mkdir_p(self, url, deadline):
    filesystem, path = self._filesystem(url)
    status, _ = filesystem.mkdir_p(
      path, timeout=self._remaining(deadline))
    status.raise_on_error()

  @staticmethod
  def _endpoint_key(url):
    parsed = urlsplit(str(url))
    return parsed.scheme, parsed.netloc

  def get(self, source, destination, timeout=None, force=False):
    """Download one URL to a local path."""
    with self._operation():
      return self._copy(source, self._local_url(destination),
                        self._deadline(timeout), force=force)

  def put(self, source, destination, timeout=None, force=False,
          create_parents=True):
    """Upload one local path, optionally creating destination parents."""
    with self._operation():
      deadline = self._deadline(timeout)
      parsed = urlsplit(str(destination))
      if create_parents and not parsed.query:
        parent = posixpath.dirname(parsed.path)
        if parent and parent != '/':
          parent_url = urlunsplit(parsed._replace(
            path=parent, query='', fragment=''))
          self._mkdir_p(parent_url, deadline)
      return self._copy(self._local_url(source), destination, deadline,
                        force=force)

  def delete(self, url, timeout=None):
    """Delete a file or collection; partial WebDAV deletes raise an error."""
    with self._operation():
      filesystem, path = self._filesystem(url)
      status, _ = filesystem.rm(path, timeout=self._remaining(
        self._deadline(timeout)))
      status.raise_on_error()

  def move(self, source, destination, timeout=None, create_parents=True):
    """Atomically rename a resource within one storage endpoint."""
    with self._operation():
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
        self._mkdir_p(parent_url, deadline)
        perform_move()

  def stat(self, url, timeout=None):
    """Return normalized metadata for a remote resource."""
    with self._operation():
      return StorageInfo(self._stat(url, self._deadline(timeout)))

  def probe(self, url, timeout=None):
    """Verify bounded endpoint access by obtaining metadata for ``url``."""
    return self.stat(url, timeout=timeout)

  def info(self, url, checksum_algorithms=None, timeout=None,
           require_checksum=False):
    """Return metadata and negotiate a supported checksum under one deadline.

    ``checksum_algorithms`` is an ordered iterable of algorithms accepted by
    the application. Successful negotiation is cached per endpoint. When
    ``require_checksum`` is true, failure to obtain an accepted checksum raises
    :class:`XRootDChecksumError`.
    """
    with self._operation():
      deadline = self._deadline(timeout)
      stat = self._stat(url, deadline)
      algorithms = []
      for algorithm in checksum_algorithms or ():
        algorithm = str(algorithm).lower()
        if algorithm not in algorithms:
          algorithms.append(algorithm)
      if not algorithms:
        return StorageInfo(stat)

      endpoint = self._endpoint_key(url)
      with self.__checksum_lock:
        cached = self.__checksum_algorithms.get(endpoint)
      if cached in algorithms:
        algorithms.remove(cached)
        algorithms.insert(0, cached)

      errors = []
      for algorithm in algorithms:
        try:
          checksum = self._checksum(url, algorithm, deadline)
        except (XRootDChecksumError, XRootDOperationError,
                XRootDUnsupportedError, ValueError) as error:
          errors.append(error)
          continue
        returned = str(checksum.algorithm).lower()
        if returned in algorithms:
          checksum.algorithm = returned
          with self.__checksum_lock:
            self.__checksum_algorithms[endpoint] = returned
          return StorageInfo(stat, checksum=checksum,
                             checksum_errors=errors)
        errors.append(ValueError(
          'endpoint returned unsupported checksum {}'.format(returned)))

      if require_checksum:
        details = '; '.join(str(error) for error in errors)
        raise XRootDChecksumError(
          details or 'no accepted checksum was returned')
      return StorageInfo(stat, checksum_errors=errors)

  def checksum(self, url, algorithm=None, timeout=None):
    """Return the checksum algorithm and value for a remote resource.

    :param algorithm: preferred checksum algorithm, such as ``adler32``;
                      endpoints may return another supported algorithm
    :returns: a two-item ``(algorithm, value)`` tuple
    """
    with self._operation():
      response = self._checksum(url, algorithm, self._deadline(timeout))
      return response.algorithm, response.value

  def exists(self, url, timeout=None):
    """Return whether a remote resource exists."""
    with self._operation():
      try:
        self._stat(url, self._deadline(timeout))
        return True
      except XRootDNotFoundError:
        return False

  def listdir(self, url, timeout=None):
    """Return a remote directory listing with entry metadata."""
    with self._operation():
      filesystem, path = self._filesystem(url)
      status, entries = filesystem.dirlist(path, timeout=self._remaining(
        self._deadline(timeout)))
      status.raise_on_error()
      return entries

  def mkdir_p(self, url, timeout=None):
    """Create a remote directory and all missing parents."""
    with self._operation():
      self._mkdir_p(url, self._deadline(timeout))

  def space(self, url, timeout=None):
    """Return ``total``, ``free``, and ``used`` byte counts for an endpoint."""
    with self._operation():
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

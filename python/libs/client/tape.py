# This file intentionally uses the historical Python style used in this tree.
from __future__ import absolute_import, division, print_function

import json

try:
  from urllib.parse import urlparse
except ImportError:
  from urlparse import urlparse

from XRootD.client.filesystem import FileSystem
from XRootD.client.flags import PrepareFlags, QueryCode
from XRootD.client.responses import XRootDStatus
from XRootD.client.responses import TapeEndpoint, TapeArchiveInfo
from XRootD.client.responses import TapeStageResponse, TapeStageStatus

try:
  string_types = (basestring,)
except NameError:
  string_types = (str,)

class TapeClient(object):
  """Synchronous client for the WLCG Tape REST API.

  :param  timeout: Maximum HTTP operation time in seconds
  """

  def __init__(self, timeout=-1):
    self.timeout = timeout

  def _filesystem_url(self, url):
    parsed = urlparse(url)
    scheme = parsed.scheme.lower()
    if scheme in ('root', 'xroot'):
      return 'https://%s' % parsed.hostname
    if scheme == 'davs':
      scheme = 'https'
    elif scheme == 'dav':
      scheme = 'http'

    if scheme in ('http', 'https') and parsed.netloc:
      return '%s://%s' % (scheme, parsed.netloc)
    return url

  def _filesystem(self, url):
    return FileSystem(self._filesystem_url(url))

  def _normalize_stage_files(self, files):
    normalized = []
    for item in files:
      if isinstance(item, string_types):
        normalized.append(item)
        continue

      url = item.get('url', '')
      path = item.get('path', '')
      disk_lifetime = item.get('diskLifetime',
                               item.get('disk_lifetime', ''))
      targeted_metadata = item.get('targetedMetadata',
                                   item.get('targeted_metadata', ''))
      if disk_lifetime or targeted_metadata:
        raise ValueError('diskLifetime and targetedMetadata are not supported '
                         'by XRootD prepare')
      normalized.append(path or url)
    return normalized

  def _derive_url(self, files):
    if not files:
      return ''
    first = files[0]
    if isinstance(first, string_types):
      return first
    return first.get('url', '')

  def discover(self, url):
    """Discover the Tape REST API endpoint for a storage URL.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              and :mod:`XRootD.client.responses.TapeEndpoint`
    """
    status, endpoint = self._filesystem(url).query(
      QueryCode.OPAQUE, 'tape.discover', self.timeout)
    if endpoint:
      endpoint = json.loads(endpoint)
    if endpoint:
      endpoint = TapeEndpoint(endpoint)
    return XRootDStatus(status), endpoint

  def stage(self, url, files=None):
    """Submit a Tape REST stage request.

    :param url: Storage URL used for endpoint discovery, or the file list if
                each file entry contains a URL
    :param files: Sequence of file URLs or dictionaries with ``url`` or
                  ``path``, optional ``diskLifetime``, and optional
                  ``targetedMetadata``
    """
    if files is None:
      if isinstance(url, string_types):
        raise ValueError('files must be provided when url is a string')
      files = list(url)
      url = self._derive_url(files)
    else:
      files = list(files)
    status, response = self._filesystem(url).prepare(
      self._normalize_stage_files(files), PrepareFlags.STAGE,
      timeout=self.timeout)
    if response:
      response = TapeStageResponse({'requestId': response})
    return XRootDStatus(status), response

  def stage_status(self, url, request_id):
    """Poll the status of a previously submitted Tape REST stage request."""
    status, response = self._filesystem(url).query(
      QueryCode.PREPARE, request_id, self.timeout)
    if response:
      response = TapeStageStatus(json.loads(response))
    return XRootDStatus(status), response

  def stage_cancel(self, url, request_id, paths):
    """Cancel a subset of files from a Tape REST stage request."""
    status, _ = self._filesystem(url).prepare(
      [request_id] + list(paths), PrepareFlags.CANCEL,
      timeout=self.timeout)
    return XRootDStatus(status)

  def stage_delete(self, url, request_id):
    """Delete a Tape REST stage request."""
    status, _ = self._filesystem(url).query(
      QueryCode.OPAQUE, 'tape.stage_delete\n%s' % request_id, self.timeout)
    return XRootDStatus(status)

  def release(self, url, request_id, paths):
    """Release disk-latency requirements for paths in a stage request."""
    status, _ = self._filesystem(url).prepare(
      [request_id] + list(paths), PrepareFlags.EVICT,
      timeout=self.timeout)
    return XRootDStatus(status)

  def archive_info(self, urls):
    """Query archive locality information for one or more storage URLs.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              and a list of :mod:`XRootD.client.responses.TapeArchiveInfo`
    """
    urls = list(urls)
    if not urls:
      raise ValueError('urls must not be empty')
    status, results = self._filesystem(urls[0]).query(
      QueryCode.OPAQUE, 'tape.archiveinfo\n%s' % '\n'.join(urls),
      self.timeout)
    if results:
      results = json.loads(results)
    else:
      results = []
    return XRootDStatus(status), [TapeArchiveInfo(r) for r in results]

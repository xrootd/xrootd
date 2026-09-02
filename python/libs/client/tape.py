# This file intentionally uses the historical Python style used in this tree.
from __future__ import absolute_import, division, print_function

import json

try:
  from urllib.parse import urlparse
except ImportError:
  from urlparse import urlparse

from XRootD.client.filesystem import FileSystem
from XRootD.client.flags import PrepareFlags, QueryCode
from XRootD.client.responses import TapeEndpoint, TapeArchiveInfo
from XRootD.client.responses import TapeStageResponse, TapeStageStatus

try:
  string_types = (basestring,)
except NameError:
  string_types = (str,)

try:
  integer_types = (int, long)
except NameError:
  integer_types = (int,)

_STRUCTURED_STAGE_PREFIX = 'xrdclhttp.tape.stage:'


def _response_text(response):
  if isinstance(response, bytes):
    response = response.decode('utf-8')
  return response.rstrip('\0')


def _reject_line_breaks(value, name):
  if '\n' in value or '\r' in value:
    raise ValueError('%s must not contain line breaks' % name)


def _normalize_disk_lifetime(value):
  if value is None:
    return None
  if isinstance(value, bool):
    raise ValueError('diskLifetime must be an ISO-8601 duration or seconds')
  if isinstance(value, integer_types):
    if value < 0:
      raise ValueError('diskLifetime seconds must not be negative')
    return 'PT%dS' % value
  if not isinstance(value, string_types) or not value:
    raise ValueError('diskLifetime must be an ISO-8601 duration or seconds')
  return value


class TapeClient(object):
  """Synchronous client for the WLCG Tape REST API.

  :param  timeout: Maximum HTTP operation time in seconds; zero uses the
                   configured XRootD default
  """

  def __init__(self, timeout=0):
    self.timeout = timeout

  def _filesystem_url(self, url):
    parsed = urlparse(url)
    scheme = parsed.scheme.lower()
    if scheme == 'davs':
      scheme = 'https'
    elif scheme == 'dav':
      scheme = 'http'

    supported = ('root', 'roots', 'xroot', 'xroots', 'http', 'https')
    if scheme in supported and parsed.netloc:
      return '%s://%s' % (scheme, parsed.netloc)
    return url

  def _operation_url(self, url):
    parsed = urlparse(url)
    scheme = parsed.scheme.lower()
    supported = ('root', 'roots', 'xroot', 'xroots',
                 'dav', 'davs', 'http', 'https')
    if scheme not in supported or not parsed.netloc:
      return url

    endpoint = self._filesystem_url(url)
    path = parsed.path or '/'
    if not path.startswith('/'):
      path = '/' + path
    return endpoint + path

  def _filesystem(self, url):
    return FileSystem(self._filesystem_url(url))

  def _is_url(self, value):
    parsed = urlparse(value)
    return bool(parsed.scheme and parsed.netloc)

  def _normalize_targeted_metadata(self, targeted_metadata):
    if targeted_metadata is None:
      return None
    if isinstance(targeted_metadata, string_types):
      targeted_metadata = json.loads(targeted_metadata)
    if not isinstance(targeted_metadata, dict):
      raise ValueError('targetedMetadata must be a JSON object')
    return targeted_metadata

  def _stage_entry(self, entry, disk_lifetime=None, targeted_metadata=None):
    payload = {}
    if self._is_url(entry):
      payload['url'] = self._operation_url(entry)
    else:
      payload['path'] = entry
    if disk_lifetime is not None:
      payload['diskLifetime'] = _normalize_disk_lifetime(disk_lifetime)
    if targeted_metadata is not None:
      payload['targetedMetadata'] = targeted_metadata
    return _STRUCTURED_STAGE_PREFIX + json.dumps(payload, sort_keys=True)

  def _normalize_stage_files(self, files, disk_lifetime=None,
                             targeted_metadata=None):
    normalized = []
    targeted_metadata = self._normalize_targeted_metadata(targeted_metadata)
    for item in files:
      if isinstance(item, string_types):
        entry = item
        entry_disk_lifetime = disk_lifetime
        entry_metadata = targeted_metadata
      else:
        url = item.get('url', '')
        path = item.get('path', '')
        entry = path or url
        if not entry:
          raise ValueError('stage file entries must contain path or url')
        entry_disk_lifetime = item.get('diskLifetime',
                                       item.get('disk_lifetime',
                                                disk_lifetime))
        entry_metadata = self._normalize_targeted_metadata(
          item.get('targetedMetadata',
                   item.get('targeted_metadata', targeted_metadata)))

      _reject_line_breaks(entry, 'stage file')
      if entry_disk_lifetime is None and entry_metadata is None:
        normalized.append(self._operation_url(entry) if self._is_url(entry)
                          else entry)
      else:
        normalized.append(self._stage_entry(
          entry, entry_disk_lifetime, entry_metadata))
    return normalized

  def _derive_url(self, files):
    if not files:
      return ''
    first = files[0]
    if isinstance(first, string_types):
      return first
    return first.get('url', '')

  def _prepare_paths(self, url, request_id, paths, flags):
    _reject_line_breaks(request_id, 'request_id')
    if isinstance(paths, string_types):
      paths = [paths]
    files = [request_id]
    files.extend(self._operation_url(path) if self._is_url(path) else path
                 for path in paths)
    status, _ = self._filesystem(url).prepare(
      files, flags, timeout=self.timeout)
    return status

  def discover(self, url):
    """Discover the Tape REST API endpoint for a storage URL.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              and :mod:`XRootD.client.responses.TapeEndpoint`
    """
    status, endpoint = self._filesystem(url).query(
      QueryCode.OPAQUE, 'tape.discover', self.timeout)
    if endpoint:
      endpoint = json.loads(_response_text(endpoint))
    if endpoint:
      endpoint = TapeEndpoint(endpoint)
    return status, endpoint

  def stage(self, url, files=None, disk_lifetime=None,
            targeted_metadata=None):
    """Submit a Tape REST stage request.

    :param url: Storage URL used for endpoint discovery and, when ``files`` is
                omitted, the single file to stage; alternatively, a file list
                whose entries contain URLs
    :param files: Sequence of file URLs or dictionaries with ``url`` or
                  ``path``, optional ``diskLifetime``, and optional
                  ``targeted_metadata``
    :param disk_lifetime: Optional disk lifetime to apply to all files
    :param targeted_metadata: Optional targeted metadata to apply to all files
    """
    if files is None:
      if isinstance(url, string_types):
        files = [url]
      else:
        files = list(url)
        url = self._derive_url(files)
        if not self._is_url(url):
          raise ValueError('url must be provided when file entries do not '
                           'contain URLs')
    elif isinstance(files, (string_types, dict)):
      files = [files]
    else:
      files = list(files)
    status, response = self._filesystem(url).prepare(
      self._normalize_stage_files(files, disk_lifetime, targeted_metadata),
      PrepareFlags.STAGE,
      timeout=self.timeout)
    if response:
      response = TapeStageResponse({'requestId': _response_text(response)})
    return status, response

  def stage_status(self, url, request_id):
    """Poll the status of a previously submitted Tape REST stage request."""
    _reject_line_breaks(request_id, 'request_id')
    status, response = self._filesystem(url).query(
      QueryCode.PREPARE, request_id, self.timeout)
    if response:
      response = TapeStageStatus(json.loads(_response_text(response)))
    return status, response

  def stage_cancel(self, url, request_id, paths):
    """Cancel a subset of files from a Tape REST stage request."""
    return self._prepare_paths(
      url, request_id, paths, PrepareFlags.CANCEL)

  def stage_delete(self, url, request_id):
    """Delete a Tape REST stage request."""
    _reject_line_breaks(request_id, 'request_id')
    status, _ = self._filesystem(url).query(
      QueryCode.OPAQUE, 'tape.stage_delete\n%s' % request_id, self.timeout)
    return status

  def release(self, url, request_id, paths):
    """Release disk-latency requirements for paths in a stage request."""
    return self._prepare_paths(
      url, request_id, paths, PrepareFlags.EVICT)

  def archive_info(self, urls):
    """Query archive locality information for one or more storage URLs.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              and a list of :mod:`XRootD.client.responses.TapeArchiveInfo`
    """
    if isinstance(urls, string_types):
      urls = [urls]
    else:
      urls = list(urls)
    if not urls:
      raise ValueError('urls must not be empty')
    for url in urls:
      _reject_line_breaks(url, 'url')
    operation_urls = [self._operation_url(url) for url in urls]
    status, results = self._filesystem(urls[0]).query(
      QueryCode.OPAQUE, 'tape.archiveinfo\n%s' % '\n'.join(operation_urls),
      self.timeout)
    if results:
      results = json.loads(_response_text(results))
    else:
      results = []
    for original, result in zip(urls, results):
      result['url'] = original
    return status, [TapeArchiveInfo(r) for r in results]

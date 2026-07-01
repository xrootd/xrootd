from __future__ import absolute_import, division, print_function

import json

import pytest

from XRootD.client import tape
from XRootD.client.flags import PrepareFlags, QueryCode


def _status(ok=True):
  return {
    'ok': ok,
    'message': '',
    'error': not ok,
    'fatal': False,
    'status': 0,
    'code': 0,
    'shellcode': 0,
    'errno': 0,
  }


class FakeFileSystem(object):
  instances = []

  def __init__(self, url):
    self.url = url
    self.calls = []
    FakeFileSystem.instances.append(self)

  def query(self, query_code, arg, timeout=0):
    self.calls.append(('query', query_code, arg, timeout))
    if query_code == QueryCode.OPAQUE and arg == 'tape.discover':
      return _status(), json.dumps({
        'uri': 'https://tape.example.org/api/v1',
        'version': 'v1',
        'sitename': 'example',
      })
    if query_code == QueryCode.PREPARE:
      return _status(), json.dumps({
        'id': arg,
        'createdAt': 1,
        'startedAt': 2,
        'files': [{'path': '/store/file', 'onDisk': True}],
      })
    if query_code == QueryCode.OPAQUE and arg.startswith('tape.archiveinfo\n'):
      return _status(), json.dumps([
        {'path': '/store/file', 'locality': 'DISK_AND_TAPE'},
        {'path': '/store/missing', 'error': 'not found'},
      ])
    if query_code == QueryCode.OPAQUE and arg.startswith('tape.stage_delete\n'):
      return _status(), ''
    return _status(False), ''

  def prepare(self, files, flags, priority=0, timeout=0):
    self.calls.append(('prepare', files, flags, priority, timeout))
    if flags == PrepareFlags.STAGE:
      return _status(), 'request-1'
    return _status(), ''


@pytest.fixture(autouse=True)
def fake_filesystem(monkeypatch):
  FakeFileSystem.instances = []
  monkeypatch.setattr(tape, 'FileSystem', FakeFileSystem)


def test_discover_uses_opaque_query():
  client = tape.TapeClient(timeout=5)
  status, endpoint = client.discover('root://xrootd.example.org/store/file')

  assert status.ok
  assert endpoint.uri == 'https://tape.example.org/api/v1'
  assert FakeFileSystem.instances[0].url == 'https://xrootd.example.org'
  assert FakeFileSystem.instances[0].calls == [
    ('query', QueryCode.OPAQUE, 'tape.discover', 5),
  ]


def test_stage_requires_files_when_url_is_string():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage('root://xrootd.example.org/store/file')


def test_stage_uses_prepare_and_returns_request_id():
  client = tape.TapeClient(timeout=7)
  status, response = client.stage(
    'root://xrootd.example.org/store/file',
    [{'path': '/store/file'}])

  assert status.ok
  assert response.requestId == 'request-1'
  assert response.request_id == 'request-1'
  assert FakeFileSystem.instances[0].calls == [
    ('prepare', ['/store/file'], PrepareFlags.STAGE, 0, 7),
  ]


def test_stage_derives_endpoint_from_file_urls():
  client = tape.TapeClient(timeout=7)
  status, response = client.stage([
    {'url': 'root://xrootd.example.org/store/file'},
  ])

  assert status.ok
  assert response.requestId == 'request-1'
  assert FakeFileSystem.instances[0].url == 'https://xrootd.example.org'
  assert FakeFileSystem.instances[0].calls == [
    ('prepare', ['root://xrootd.example.org/store/file'],
     PrepareFlags.STAGE, 0, 7),
  ]


def test_stage_applies_global_disk_lifetime_and_metadata():
  client = tape.TapeClient(timeout=7)
  status, response = client.stage(
    'root://xrootd.example.org/store/file',
    ['root://xrootd.example.org/store/file'],
    disk_lifetime=3600,
    targeted_metadata={'activity': 'analysis'})

  assert status.ok
  assert response.request_id == 'request-1'
  assert FakeFileSystem.instances[0].calls == [
    ('prepare', [
      'xrdclhttp.tape.stage:'
      '{"diskLifetime": "3600", "targetedMetadata": '
      '{"activity": "analysis"}, '
      '"url": "root://xrootd.example.org/store/file"}',
    ], PrepareFlags.STAGE, 0, 7),
  ]


def test_stage_accepts_file_metadata():
  client = tape.TapeClient(timeout=7)
  status, response = client.stage(
    'root://xrootd.example.org/store/file',
    [{'path': '/store/file',
      'diskLifetime': 'PT1H',
      'targetedMetadata': {'activity': 'analysis'}}])

  assert status.ok
  assert response.request_id == 'request-1'
  assert FakeFileSystem.instances[0].calls == [
    ('prepare', [
      'xrdclhttp.tape.stage:'
      '{"diskLifetime": "PT1H", "path": "/store/file", '
      '"targetedMetadata": {"activity": "analysis"}}',
    ], PrepareFlags.STAGE, 0, 7),
  ]


def test_stage_rejects_invalid_targeted_metadata():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage('root://xrootd.example.org/store/file', [
      {'path': '/store/file', 'targetedMetadata': ['analysis']},
    ])


def test_stage_rejects_empty_file_entry():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage('root://xrootd.example.org/store/file', [{}])


def test_stage_rejects_file_entry_line_breaks():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage('root://xrootd.example.org/store/file', [
      {'path': '/store/file\nother'},
    ])

  with pytest.raises(ValueError):
    client.stage('root://xrootd.example.org/store/file', [
      'root://xrootd.example.org/store/file\nother',
    ])


def test_stage_requires_url_when_deriving_endpoint():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage([{'path': '/store/file'}])


def test_stage_status_uses_prepare_query():
  client = tape.TapeClient(timeout=3)
  status, response = client.stage_status(
    'root://xrootd.example.org/store/file', 'request-1')

  assert status.ok
  assert response.id == 'request-1'
  assert response.files[0].path == '/store/file'
  assert response.files[0].onDisk
  assert response.files[0].on_disk
  assert response.file_status('/store/file') is response.files[0]
  assert response.file_status('root://xrootd.example.org//store/file') \
    is response.files[0]
  assert response.is_on_disk('/store/file')
  assert not response.is_on_disk('/store/missing')
  assert FakeFileSystem.instances[0].calls == [
    ('query', QueryCode.PREPARE, 'request-1', 3),
  ]


def test_stage_status_rejects_line_break_in_request_id():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage_status('root://xrootd.example.org/store/file',
                        'request-1\nother')


def test_cancel_delete_release_and_archive_info():
  client = tape.TapeClient(timeout=11)

  assert client.stage_cancel(
    'root://xrootd.example.org/store/file', 'request-1',
    ['/store/file']).ok
  assert client.stage_delete(
    'root://xrootd.example.org/store/file', 'request-1').ok
  assert client.release(
    'root://xrootd.example.org/store/file', 'request-1',
    ['/store/file']).ok
  status, archive_info = client.archive_info([
    'root://xrootd.example.org/store/file',
    'root://xrootd.example.org/store/missing',
  ])

  assert status.ok
  assert archive_info[0].locality == 'DISK_AND_TAPE'
  assert archive_info[1].error == 'not found'

  assert FakeFileSystem.instances[0].calls == [
    ('prepare', ['request-1', '/store/file'], PrepareFlags.CANCEL, 0, 11),
  ]
  assert FakeFileSystem.instances[1].calls == [
    ('query', QueryCode.OPAQUE, 'tape.stage_delete\nrequest-1', 11),
  ]
  assert FakeFileSystem.instances[2].calls == [
    ('prepare', ['request-1', '/store/file'], PrepareFlags.EVICT, 0, 11),
  ]
  assert FakeFileSystem.instances[3].calls == [
    ('query', QueryCode.OPAQUE,
     'tape.archiveinfo\nroot://xrootd.example.org/store/file\n'
     'root://xrootd.example.org/store/missing', 11),
  ]


def test_opaque_helpers_reject_line_breaks():
  client = tape.TapeClient()

  with pytest.raises(ValueError):
    client.stage_delete('root://xrootd.example.org/store/file',
                        'request-1\nother')

  with pytest.raises(ValueError):
    client.stage_cancel('root://xrootd.example.org/store/file',
                        'request-1\nother', ['/store/file'])

  with pytest.raises(ValueError):
    client.release('root://xrootd.example.org/store/file',
                   'request-1\nother', ['/store/file'])

  with pytest.raises(ValueError):
    client.archive_info([
      'root://xrootd.example.org/store/file\nother',
    ])

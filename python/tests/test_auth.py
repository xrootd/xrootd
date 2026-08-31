from __future__ import absolute_import, division, print_function

import gc
import os

from concurrent.futures import ThreadPoolExecutor
from urllib.parse import parse_qs, urlsplit

import pytest

from XRootD.client import AuthContext
from XRootD.client.copyprocess import CopyProcess
from XRootD.client.file import File
from XRootD.client.filesystem import FileSystem
import XRootD.client.copyprocess as copyprocess_module
import XRootD.client.file as file_module
import XRootD.client.filesystem as filesystem_module


def _query(url):
  return parse_qs(urlsplit(url).query)


def test_bearer_token_is_scoped_to_context():
  first = AuthContext.bearer(token='first-token')
  second = AuthContext.bearer(token='second-token')
  try:
    first_url = first.apply('root://localhost//data?existing=value')
    second_url = second.apply('root://localhost//data?existing=value')
    first_query = _query(first_url)
    second_query = _query(second_url)

    assert first_query['existing'] == ['value']
    assert first_query['xrd.wantprot'] == ['ztn']
    assert first_query['xrdcl.authctx'] != second_query['xrdcl.authctx']
    with open(first_query['xrd.ztn'][0]) as token_file:
      assert token_file.read() == 'first-token'
  finally:
    first.close()
    second.close()


def test_bearer_context_removes_owned_token_file():
  context = AuthContext.bearer(token='secret')
  token_file = _query(context.apply('root://localhost/'))['xrd.ztn'][0]
  assert os.path.exists(token_file)
  context.close()
  assert not os.path.exists(token_file)
  with pytest.raises(RuntimeError):
    context.apply('root://localhost/')


def test_x509_proxy_and_cert_key_parameters():
  proxy = AuthContext.x509(proxy='/tmp/proxy')
  cert_key = AuthContext.x509(cert='/tmp/cert', key='/tmp/key')

  proxy_query = _query(proxy.apply('roots://localhost//data'))
  cert_key_query = _query(cert_key.apply('root://localhost//data'))

  assert proxy_query['xrd.wantprot'] == ['gsi']
  assert proxy_query['xrd.gsiusrpxy'] == ['/tmp/proxy']
  assert cert_key_query['xrd.gsiusrcrt'] == ['/tmp/cert']
  assert cert_key_query['xrd.gsiusrkey'] == ['/tmp/key']


@pytest.mark.parametrize('arguments', (
  {},
  {'token': ''},
  {'token': 'value', 'token_file': '/tmp/token'},
))
def test_bearer_requires_one_credential(arguments):
  with pytest.raises(ValueError):
    AuthContext.bearer(**arguments)


def test_credential_paths_reject_query_separators():
  with pytest.raises(ValueError):
    AuthContext.bearer(token_file='/tmp/token&xrd.wantprot=unix')
  with pytest.raises(ValueError):
    AuthContext.x509(proxy='/tmp/proxy#fragment')


@pytest.mark.parametrize('arguments', (
  {},
  {'proxy': '/tmp/proxy', 'cert': '/tmp/cert', 'key': '/tmp/key'},
  {'cert': '/tmp/cert'},
  {'key': '/tmp/key'},
))
def test_x509_requires_one_complete_credential(arguments):
  with pytest.raises(ValueError):
    AuthContext.x509(**arguments)


def test_non_xrootd_urls_are_unchanged():
  context = AuthContext.bearer(token='secret')
  try:
    url = 'file:///tmp/data?existing=value'
    assert context.apply(url) == url
  finally:
    context.close()


def test_http_bearer_is_object_scoped_and_preserves_signed_query():
  context = AuthContext.bearer(token_file='/tmp/token file',
                               ca_file='/tmp/custom ca.pem')
  authenticated = context.apply(
    'davs://storage.example/data?X-Amz-Signature=abc+def==&empty=')
  query = _query(authenticated)

  assert 'X-Amz-Signature=abc+def==' in authenticated
  assert 'empty=' in authenticated
  assert query['xrdcl.http.bearertokenfile'] == ['/tmp/token file']
  assert query['xrdcl.http.cafile'] == ['/tmp/custom ca.pem']
  assert 'xrd.wantprot' not in query


def test_http_x509_and_anonymous_contexts():
  x509 = AuthContext.x509(proxy='/tmp/proxy', verify=False)
  anonymous = AuthContext.anonymous()

  x509_query = _query(x509.apply('https://storage.example/data'))
  anonymous_query = _query(anonymous.apply('https://storage.example/data'))
  assert x509_query['xrdcl.http.clientcert'] == ['/tmp/proxy']
  assert x509_query['xrdcl.http.clientkey'] == ['/tmp/proxy']
  assert x509_query['xrdcl.http.noverify'] == ['1']
  assert anonymous_query['xrdcl.http.noauth'] == ['1']


def test_anonymous_context_rejects_root_protocol():
  with pytest.raises(ValueError):
    AuthContext.anonymous().apply('root://storage.example//data')


def test_xroots_url_is_authenticated():
  context = AuthContext.x509(proxy='/tmp/proxy')
  assert _query(context.apply('xroots://localhost//data'))['xrd.wantprot'] == ['gsi']


def test_existing_opaque_parameters_are_preserved_verbatim():
  context = AuthContext.x509(proxy='/tmp/proxy')
  url = 'root://localhost//data?cap.sym=abc+def==&empty='
  authenticated = context.apply(url)
  assert 'cap.sym=abc+def==' in authenticated
  assert 'empty=' in authenticated


def test_context_can_be_shared_between_threads():
  context = AuthContext.x509(proxy='/tmp/proxy')
  url = 'root://localhost//data'
  with ThreadPoolExecutor(max_workers=16) as executor:
    urls = list(executor.map(context.apply, [url] * 128))
  assert len(set(urls)) == 1


def test_filesystem_applies_context_to_endpoint(monkeypatch):
  endpoints = []

  class NativeFileSystem(object):
    def __init__(self, url):
      endpoints.append(url)

  monkeypatch.setattr(filesystem_module.client, 'FileSystem', NativeFileSystem)
  context = AuthContext.x509(proxy='/tmp/proxy')
  filesystem = FileSystem('root://localhost/', auth=context)

  assert filesystem is not None
  assert _query(endpoints[0])['xrd.gsiusrpxy'] == ['/tmp/proxy']


def test_file_applies_context_when_opening(monkeypatch):
  opened = []

  class NativeFile(object):
    def open(self, url, flags, mode, timeout):
      opened.append(url)
      return {'ok': True, 'message': ''}, None

  monkeypatch.setattr(file_module.client, 'File', NativeFile)
  context = AuthContext.x509(proxy='/tmp/proxy')
  file_handle = File(auth=context)
  status, _ = file_handle.open('root://localhost//data')

  assert status.ok
  assert _query(opened[0])['xrd.gsiusrpxy'] == ['/tmp/proxy']


def test_copy_process_supports_distinct_endpoint_contexts(monkeypatch):
  jobs = []

  class NativeCopyProcess(object):
    def add_job(self, *arguments):
      jobs.append(arguments)

  monkeypatch.setattr(copyprocess_module.client, 'CopyProcess', NativeCopyProcess)
  source_auth = AuthContext.x509(proxy='/tmp/source-proxy')
  target_auth = AuthContext.bearer(token='target-token')
  try:
    copy = CopyProcess()
    copy.add_job(
      'root://source.example//data',
      'root://target.example//data',
      source_auth=source_auth,
      target_auth=target_auth,
    )

    assert _query(jobs[0][0])['xrd.gsiusrpxy'] == ['/tmp/source-proxy']
    assert _query(jobs[0][1])['xrd.wantprot'] == ['ztn']
    assert _query(jobs[0][0])['xrdcl.authctx'] != _query(jobs[0][1])['xrdcl.authctx']
  finally:
    target_auth.close()


def test_copy_process_retains_per_job_contexts(monkeypatch):
  jobs = []

  class NativeCopyProcess(object):
    def add_job(self, *arguments):
      jobs.append(arguments)

  monkeypatch.setattr(copyprocess_module.client, 'CopyProcess', NativeCopyProcess)
  copy = CopyProcess()
  copy.add_job(
    'root://source.example//data',
    'file:///tmp/data',
    source_auth=AuthContext.bearer(token='source-token'),
  )

  token_file = _query(jobs[0][0])['xrd.ztn'][0]
  gc.collect()
  assert os.path.exists(token_file)

from XRootD import client


def test_env_delete_helpers():
  assert client.EnvPutString('XROOTD_TEST_ENV_DELETE_STRING', 'temporary')
  assert client.EnvGetString('XROOTD_TEST_ENV_DELETE_STRING') == 'temporary'
  assert client.EnvDelString('XROOTD_TEST_ENV_DELETE_STRING')
  assert client.EnvGetString('XROOTD_TEST_ENV_DELETE_STRING') is None
  assert client.EnvDelString('XROOTD_TEST_ENV_DELETE_STRING')

  assert client.EnvPutInt('XROOTD_TEST_ENV_DELETE_INT', 42)
  assert client.EnvGetInt('XROOTD_TEST_ENV_DELETE_INT') == 42
  assert client.EnvDelInt('XROOTD_TEST_ENV_DELETE_INT')
  assert client.EnvGetInt('XROOTD_TEST_ENV_DELETE_INT') is None
  assert client.EnvDelInt('XROOTD_TEST_ENV_DELETE_INT')

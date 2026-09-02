import pytest

from XRootD.client.responses import XRootDStatus, XRootDNotFoundError, \
  XRootDAuthorizationError, \
  XRootDTimeoutError, XRootDChecksumError, XRootDOperationError, \
  raise_on_error


def status(code, ok=False, shellcode=0, message='error', errno=0):
  return XRootDStatus({
    'message': message,
    'ok': ok,
    'error': not ok,
    'fatal': False,
    'status': 0 if ok else 1,
    'code': code,
    'shellcode': shellcode,
    'errno': errno,
  })


def test_status_error_name_and_exceptions():
  assert status(XRootDStatus.errNotFound).error_name == 'errNotFound'
  assert status(XRootDStatus.errPipelineFailed).error_name == \
    'errPipelineFailed'
  assert status(XRootDStatus.errTlsError).error_name == 'errTlsError'
  assert status(999).error_name is None
  assert isinstance(status(XRootDStatus.errNotFound).exception(),
                    XRootDNotFoundError)
  assert isinstance(status(XRootDStatus.errAuthFailed).exception(),
                    XRootDAuthorizationError)
  assert isinstance(status(XRootDStatus.errSocketTimeout).exception(),
                    XRootDTimeoutError)
  assert isinstance(status(XRootDStatus.errCheckSumError).exception(),
                    XRootDChecksumError)
  assert isinstance(status(XRootDStatus.errUnknown).exception(),
                    XRootDOperationError)


@pytest.mark.parametrize('errno, exception_type', [
  (3011, XRootDNotFoundError),
  (3010, XRootDAuthorizationError),
  (3030, XRootDAuthorizationError),
  (3034, XRootDTimeoutError),
  (3035, XRootDTimeoutError),
  (3019, XRootDChecksumError),
  (3012, XRootDOperationError),
])
def test_server_error_exceptions(errno, exception_type):
  error = status(XRootDStatus.errErrorResponse, shellcode=54, errno=errno)
  assert isinstance(error.exception(), exception_type)


def test_raise_on_error():
  ok = status(XRootDStatus.errNone, ok=True, message='ok')
  assert raise_on_error(ok) is ok
  assert isinstance(raise_on_error(ok.__dict__), XRootDStatus)

  with pytest.raises(XRootDNotFoundError) as excinfo:
    status(XRootDStatus.errNotFound).raise_on_error()
  assert excinfo.value.status.code == XRootDStatus.errNotFound

from XRootD.client.responses import XRootDStatus, XRootDNotFoundError, \
  XRootDAuthorizationError, \
  XRootDTimeoutError, XRootDChecksumError, XRootDOperationError, \
  raise_on_error


def status(code, ok=False, shellcode=0, message='error'):
  return XRootDStatus({
    'message': message,
    'ok': ok,
    'error': not ok,
    'fatal': False,
    'status': 0 if ok else 1,
    'code': code,
    'shellcode': shellcode,
    'errno': 0,
  })


def test_status_error_name_and_exceptions():
  assert status(XRootDStatus.errNotFound).error_name == 'errNotFound'
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


def test_raise_on_error():
  ok = status(XRootDStatus.errNone, ok=True, message='ok')
  assert raise_on_error(ok) is ok

  try:
    status(XRootDStatus.errNotFound).raise_on_error()
  except XRootDNotFoundError as error:
    assert error.status.code == XRootDStatus.errNotFound
  else:
    assert False

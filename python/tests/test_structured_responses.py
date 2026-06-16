from XRootD.client.responses import ChecksumInfo, CapabilityInfo, XRootDStatus


def status(code, ok=False, message='error'):
  return XRootDStatus({
    'message': message,
    'ok': ok,
    'error': not ok,
    'fatal': False,
    'status': 0 if ok else 1,
    'code': code,
    'shellcode': 0,
    'errno': 0,
  })


def test_checksum_info():
  checksum = ChecksumInfo('adler32 deadbeef\n')
  assert checksum.algorithm == 'adler32'
  assert checksum.value == 'deadbeef'

  checksum = ChecksumInfo.from_query_response('md5 abc123\0')
  assert checksum.algorithm == 'md5'
  assert checksum.value == 'abc123'


def test_capability_info():
  ping_status = status(XRootDStatus.errNone, ok=True, message='ok')
  capability = CapabilityInfo(ping_status=ping_status)
  assert capability.ping_status.ok
  assert capability.protocol is None

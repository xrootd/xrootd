# Basic test for setXAttrAdler32 binding
import os, tempfile
from XRootD.client import setXAttrAdler32

# Create temp file
path = None
try:
    fd, path = tempfile.mkstemp()
    # write some data
    with os.fdopen(fd, 'wb') as f:
        f.write(b'Testing adler32 binding')

    # Use dummy checksum value of length 8 (not validated for correctness here)
    chk = '01234567'

    # Call binding with simplified API (only path and checksum)
    setXAttrAdler32(path, chk)
    print('Called setXAttrAdler32 successfully on', path)

    # Verify structured attribute existence via listxattr
    try:
        import xattr
        # The structured attribute created via XrdCksXAttr will have name 'user.XrdCks.adler32'
        xas = xattr.listxattr(path)
        print('xattrs after call:', xas)
        if 'user.XrdCks.adler32' in xas:
            print('PASS: structured checksum attribute present')
        else:
            print('WARN: expected user.XrdCks.adler32 not found')
    except Exception as e:
        print('xattr verification skipped:', e)
finally:
    if path and os.path.exists(path):
        os.unlink(path)

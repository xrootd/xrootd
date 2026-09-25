#!/usr/bin/env python3
"""Check installed prepare artifacts and their RPM/Debian package ownership."""
# Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
from pathlib import Path
import sys

source, prefix = (Path(arg) for arg in sys.argv[1:3])
lib = next(p for p in (prefix / 'lib64', prefix / 'lib')
           if (p / 'libXrdOfsPrepPersist.so').exists())
for name in ('XrdOfsPrepPersist.hh', 'XrdOfsPrepStorage.hh',
             'XrdOfsPrepBackend.hh', 'XrdOfsPrepare.hh'):
    assert (prefix / 'include/xrootd/XrdOfs' / name).is_file(), name
for name in ('libXrdOfsPrepPersist.so', 'libXrdOfsPrepPersist.so.1',
             'libXrdOfsPrepPersist-6.so'):
    assert (lib / name).exists(), name
spec = (source / 'xrootd.spec').read_text()
for section, entry in (
        ('server-libs', '%{_libdir}/libXrdOfsPrepPersist.so.*'),
        ('server-libs', '%{_libdir}/libXrdOfsPrepPersist-6.so'),
        ('server-devel', '%{_libdir}/libXrdOfsPrepPersist.so'),
        ('server-devel', '%{_includedir}/%{name}/XrdOfs')):
    body = spec.split('%files ' + section + '\n', 1)[1].split('%files', 1)[0]
    assert entry in body.splitlines(), (section, entry)
for package, entry in (
        ('libxrdserver6', '/usr/lib/*/libXrdOfsPrepPersist.so.*'),
        ('libxrdserver6t64', '/usr/lib/*/libXrdOfsPrepPersist.so.*'),
        ('xrootd-server-plugins', '/usr/lib/*/libXrdOfsPrepPersist-6.so'),
        ('libxrootd-server-dev', '/usr/lib/*/libXrdOfsPrepPersist.so'),
        ('libxrootd-server-dev', '/usr/include/xrootd/XrdOfs')):
    entries = (source / 'debian' / (package + '.install')).read_text()
    assert entry in entries.splitlines(), (package, entry)
print('Installed headers, SONAME/plugin and RPM/Debian ownership: PASS')

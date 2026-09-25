#!/usr/bin/env python3
"""Verify OFS passes each native prepare path's CGI to its real authorizer."""
# Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

BUILD, AUTH = (Path(arg).resolve() for arg in sys.argv[1:3])
del sys.argv[1:3]


class Authorization(unittest.TestCase):
    def test_per_path_cgi(self):
        with tempfile.TemporaryDirectory(prefix='xrd-ofs-auth-') as directory:
            root = Path(directory)
            with socket.socket() as sock:
                sock.bind(('127.0.0.1', 0))
                port = sock.getsockname()[1]
            config = root / 'xrootd.cfg'
            config.write_text(f'''
xrd.port {port}
xrd.network nodnr
xrd.allow host 127.0.0.1
all.adminpath {root}/admin
all.pidpath {root}/admin
all.export /
oss.localroot {root}
xrootd.seclib libXrdSec.so
sec.protocol unix
sec.protbind * only unix
ofs.authorize 1
ofs.authlib {AUTH}
ofs.preplib libXrdOfsPrepGPI.so -admit stage -run /bin/true
''')
            env = dict(os.environ, LD_LIBRARY_PATH=str(BUILD / 'lib'),
                       XRD_CONNECTIONRETRY='0', XRD_REQUESTTIMEOUT='5')
            env.pop('BEARER_TOKEN', None)
            env.pop('BEARER_TOKEN_FILE', None)
            with (root / 'server.log').open('w') as log:
                server = subprocess.Popen(
                    [str(BUILD / 'bin/xrootd'), '-c', str(config)],
                    env=env, stdout=log, stderr=subprocess.STDOUT,
                    start_new_session=True, cwd=root)
                try:
                    for _ in range(100):
                        self.assertIsNone(server.poll())
                        try:
                            with socket.create_connection(
                                    ('127.0.0.1', port), timeout=0.1):
                                break
                        except OSError:
                            time.sleep(0.05)
                    for paths, accepted in (
                            (('/a?authz=alpha', '/b?authz=beta'), True),
                            (('/a?authz=alpha', '/b?authz=alpha'), False),
                            (('/a?authz=alpha', '/b'), False),
                            (('/a', '/b?authz=beta'), False)):
                        with self.subTest(paths=paths):
                            result = subprocess.run(
                                [str(BUILD / 'bin/xrdfs'),
                                 f'root://127.0.0.1:{port}', 'prepare', '-s',
                                 *paths], env=env, capture_output=True,
                                text=True, timeout=15)
                            self.assertEqual(result.returncode == 0, accepted,
                                             result.stdout + result.stderr)
                finally:
                    os.killpg(server.pid, signal.SIGTERM)
                    server.wait(timeout=10)
                    log.flush()
                    print((root / 'server.log').read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)

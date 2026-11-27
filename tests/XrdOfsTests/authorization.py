#!/usr/bin/env python3
"""Verify OFS isolates each native prepare path's authorization context."""
# Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
import os
from pathlib import Path
import signal
import shutil
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
        prepare_program = shutil.which('true')
        self.assertIsNotNone(prepare_program)
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
ofs.preplib libXrdOfsPrepGPI.so -admit stage -run {prepare_program}
''')
            env = dict(os.environ, XRD_CONNECTIONRETRY='0',
                       XRD_REQUESTTIMEOUT='5')
            for name in ('LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH'):
                env[name] = os.pathsep.join(filter(None, (
                    str(BUILD / 'lib'), env.get(name))))
            env.pop('BEARER_TOKEN', None)
            env.pop('BEARER_TOKEN_FILE', None)
            with (root / 'server.log').open('w') as log:
                server = subprocess.Popen(
                    [str(BUILD / 'bin/xrootd'), '-c', str(config)],
                    env=env, stdout=log, stderr=subprocess.STDOUT,
                    start_new_session=True, cwd=root)
                try:
                    for _ in range(100):
                        self.assertIsNone(server.poll(),
                                          'Server exited during startup')
                        try:
                            with socket.create_connection(
                                    ('127.0.0.1', port), timeout=0.1):
                                break
                        except OSError:
                            time.sleep(0.05)
                    else:
                        self.fail('Server did not start listening')
                    for paths, accepted in (
                            (('/a?authz=alpha', '/b?authz=beta'), True),
                            (('/b?authz=beta', '/a?authz=alpha'), True),
                            (('/a?authz=beta', '/b?authz=alpha'), False),
                            (('/a?authz=alpha', '/b?authz=alpha'), False),
                            (('/a?authz=alpha', '/b'), False),
                            (('/a', '/b?authz=beta'), False),
                            (('/a?authz=alpha', '/inherited'), False)):
                        with self.subTest(paths=paths):
                            result = subprocess.run(
                                [str(BUILD / 'bin/xrdfs'),
                                 f'root://127.0.0.1:{port}', 'prepare', '-s',
                                 *paths], env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT,
                                universal_newlines=True, timeout=15)
                            self.assertEqual(result.returncode == 0, accepted,
                                             result.stdout)
                finally:
                    # A startup failure may already have reaped the process.
                    # Always print its log without hiding the original failure.
                    failure = sys.exc_info()[0] is not None
                    try:
                        if server.poll() is None:
                            os.killpg(server.pid, signal.SIGTERM)
                        try:
                            server.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            os.killpg(server.pid, signal.SIGKILL)
                            server.wait(timeout=5)
                    except (OSError, subprocess.TimeoutExpired) as error:
                        print(f'Server cleanup failed: {error}',
                              file=sys.stderr)
                        if not failure:
                            raise
                    finally:
                        log.flush()
                        print((root / 'server.log').read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)

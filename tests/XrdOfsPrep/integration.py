#!/usr/bin/env python3
"""Loopback-only HTTP -> bridge -> OFS -> persistent wrapper -> GPI test.

Uses synthetic TestAuth credentials; the SciTokens fixture tests real tokens.
No external tape, credentials, container or third-party Python needed.
"""

# Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

from mock_tape import atomic_json

BUILD = Path(sys.argv.pop(1)).resolve()
AUTH = Path(sys.argv.pop(1)).resolve()
SOURCE = Path(__file__).resolve().parent


class Integration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='xrd-prep-int-')
        cls.root = Path(cls.directory.name)
        cls.tape = cls.root / 'tape'
        for name in ('archive', 'disk', 'registry'):
            (cls.tape / name).mkdir(parents=True)
        (cls.tape / 'archive' / 'a').write_text('tape a')
        (cls.tape / 'archive' / 'b').write_text('tape b')
        (cls.tape / 'archive' / 'release-only').write_text('release data')
        for i in range(48):
            (cls.tape / 'archive' / f'file_{i:02d}').write_text(f'tape {i}')
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            cls.port = sock.getsockname()[1]
        cls.env = dict(
            os.environ,
            XRD_PREP_MOCK_ROOT=str(cls.tape),
            LD_LIBRARY_PATH=str(BUILD / 'lib')
            + ':'
            + os.environ.get('LD_LIBRARY_PATH', ''),
            DYLD_LIBRARY_PATH=str(BUILD / 'lib')
            + ':'
            + os.environ.get('DYLD_LIBRARY_PATH', ''),
            XRD_CONNECTIONRETRY='0',
            XRD_REQUESTTIMEOUT='10',
        )
        cls.env.pop('BEARER_TOKEN', None)
        cls.env.pop('BEARER_TOKEN_FILE', None)
        backend = (
            'ofs.preplib libXrdOfsPrepGPI.so '
            '-admit stage,query,cancel,evict -cgi -maxfiles 48 -maxresp 1m '
            f'-run {SOURCE}/mock_tape.py'
        )
        persistence = (
            'ofs.preplib ++ libXrdOfsPrepPersist.so '
            f'{cls.tape}/registry prepare-test replay-safe'
        )
        handler = BUILD / 'lib/libXrdOfsPrepTestHttpHandler.so'
        cls.config = cls.root / 'xrootd.cfg'
        cls.config.write_text(f"""
xrd.port {cls.port}
xrd.protocol http:{cls.port} libXrdHttp.so
xrd.network nodnr
xrd.allow host 127.0.0.1
http.httpsmode disable
http.header2cgi Authorization authz
all.adminpath {cls.root}/admin
all.pidpath {cls.root}/admin
all.sitename prepare-test
all.export /
oss.localroot {cls.tape}/disk
xrootd.seclib libXrdSec.so
sec.protocol unix
sec.protbind * only unix
ofs.authorize 1
ofs.authlib {AUTH}
{backend}
{persistence}
http.exthandler legacy +notls {handler}
http.exthandler overflow +notls {handler} bridge
http.exthandler xrdhttptapeapi +notls libXrdHttpTapeApi.so
""")
        cls.process = None
        cls.log = (cls.root / 'server.log').open('a')
        try:
            cls.start()
        except Exception:
            cls.dump_log()
            cls.stop()
            raise

    @classmethod
    def _wait_for_port_release(cls):
        for _ in range(50):
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    s.bind(('127.0.0.1', cls.port))
                    return
            except OSError:
                time.sleep(0.1)
        raise RuntimeError(f'port {cls.port} did not become available')

    @classmethod
    def start(cls):
        for attempt in range(5):
            cls._wait_for_port_release()
            cls.process = subprocess.Popen(
                [
                    str(BUILD / 'bin' / 'xrootd'),
                    '-c',
                    str(cls.config),
                    '-n',
                    'prepare-test',
                ],
                env=cls.env,
                cwd=cls.root,
                stdout=cls.log,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            for _ in range(150):
                if cls.process.poll() is not None:
                    break
                try:
                    conn = http.client.HTTPConnection(
                        '127.0.0.1', cls.port, timeout=0.5
                    )
                    conn.request('GET', '/.well-known/wlcg-tape-rest-api')
                    res = conn.getresponse()
                    res.read()
                    conn.close()
                    if res.status == 200:
                        return
                    time.sleep(0.1)
                except (OSError, http.client.HTTPException):
                    time.sleep(0.1)
            cls.stop(crash=True)
            time.sleep(0.5)
        raise RuntimeError('XRootD startup timed out')

    @classmethod
    def stop(cls, crash=False):
        if cls.process and cls.process.poll() is None:
            sig = signal.SIGKILL if crash else signal.SIGTERM
            try:
                os.killpg(os.getpgid(cls.process.pid), sig)
            except (ProcessLookupError, OSError):
                cls.process.kill() if crash else cls.process.terminate()
            try:
                cls.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(cls.process.pid), signal.SIGKILL)
                except (ProcessLookupError, OSError):
                    cls.process.kill()
                cls.process.wait()
        cls._wait_for_port_release()

    @classmethod
    def dump_log(cls):
        cls.log.flush()
        print((cls.root / 'server.log').read_text(), file=sys.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.stop()
        cls.dump_log()
        cls.log.close()
        cls.directory.cleanup()

    def request(
        self,
        method,
        path,
        body=None,
        token='alice',
        connection=None,
        raw=False,
    ):
        headers = {}
        if token is not None:
            headers['Authorization'] = 'Bearer ' + token
        if body is not None:
            if not raw:
                body = json.dumps(body)
            headers['Content-Type'] = 'application/json'
        max_attempts = 10 if method == 'GET' else 1
        for attempt in range(max_attempts):
            conn = connection or http.client.HTTPConnection(
                '127.0.0.1', self.port, timeout=15
            )
            try:
                conn.request(method, path, body, headers)
                response = conn.getresponse()
                payload = response.read().decode()
                return (
                    response.status,
                    dict(response.getheaders()),
                    json.loads(payload) if payload else None,
                )
            except (
                ConnectionResetError,
                ConnectionRefusedError,
                http.client.RemoteDisconnected,
                BrokenPipeError,
            ):
                if connection is not None or attempt == max_attempts - 1:
                    raise
                time.sleep(0.2)
            finally:
                if connection is None:
                    conn.close()

    def wait_for(self, predicate, timeout=20):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if predicate():
                return
            time.sleep(0.1)
        self.fail('condition did not complete before timeout')

    def stage(self, files=None):
        result = None

        def admitted():
            nonlocal result
            result = self.request(
                'POST', '/api/v1/stage', {'files': files or [{'path': '/a'}]}
            )
            return result[0] != 503

        self.wait_for(admitted)
        self.assertEqual(result[0], 201, result)
        request_id = result[2]['requestId']
        self.assertRegex(request_id, r'^[0-9a-f-]{36}$')
        self.assertTrue(result[1]['Location'].endswith(request_id))
        return request_id

    def status(self, request_id):
        status, _, body = self.request('GET', '/api/v1/stage/' + request_id)
        self.assertEqual(status, 200, body)
        return body

    def wait_state(self, request_id, state):
        self.wait_for(
            lambda: all(
                f['state'] == state for f in self.status(request_id)['files']
            )
        )

    def test_01_discovery_and_authorization(self):
        status, _, _ = self.request(
            'GET', '/.well-known/wlcg-tape-rest-api', token=None
        )
        self.assertEqual(status, 200)
        status, headers, body = self.request(
            'POST', '/api/v1/stage', {'files': [{'path': '/a'}]}, token=None
        )
        self.assertEqual(status, 403, body)
        self.assertEqual(headers['Content-Type'], 'application/problem+json')

    def test_02_lifecycle_metadata_owner_and_keepalive(self):
        files = [
            {
                'path': '/a',
                'diskLifetime': 'PT1H',
                'targetedMetadata': {'test': {'value': 1}},
            },
            {'path': '/b', 'diskLifetime': 'PT2H'},
        ]
        request_id = self.stage(files)
        self.wait_state(request_id, 'COMPLETED')
        backend_record = (
            self.tape
            / 'backend'
            / 'requests'
            / request_id[:2]
            / request_id[2:4]
            / (request_id + '.json')
        )
        backend = json.loads(backend_record.read_text())
        self.assertEqual(
            [f['diskLifetime'] for f in backend['files']], ['PT1H', 'PT2H']
        )
        record = (
            self.tape
            / 'registry'
            / 'requests'
            / request_id[:2]
            / request_id[2:4]
            / (request_id + '.json')
        )
        self.assertNotIn('authz', record.read_text())
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        try:
            for token, expected in [
                ('alice', 200),
                ('bob', 403),
                (None, 403),
                ('alice', 200),
            ]:
                self.assertEqual(
                    self.request(
                        'GET',
                        '/api/v1/stage/' + request_id,
                        token=token,
                        connection=conn,
                    )[0],
                    expected,
                )
        finally:
            conn.close()
        self.assertEqual(
            self.request(
                'POST',
                '/api/v1/stage/' + request_id + '/cancel',
                {'paths': ['/a', '/not-member']},
            )[0],
            400,
        )
        # Delete is a tombstone, not deletion of tape or disk data.
        self.assertEqual(
            self.request('DELETE', '/api/v1/stage/' + request_id)[0], 200
        )
        self.assertEqual(
            self.request('GET', '/api/v1/stage/' + request_id)[0], 404
        )
        self.assertTrue((self.tape / 'archive' / 'a').exists())
        self.assertTrue((self.tape / 'disk' / 'a').exists())

    def test_03_native_and_http_share_backend(self):
        url = f'root://127.0.0.1:{self.port}'
        result = subprocess.run(
            [str(BUILD / 'bin' / 'xrdfs'), 'prepare', '-s', url + '//a'],
            env=self.env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=20,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        request_id = result.stdout.strip()
        self.wait_state(request_id, 'COMPLETED')
        query = subprocess.run(
            [
                str(BUILD / 'bin' / 'xrdfs'),
                url,
                'query',
                'prepare',
                request_id,
            ],
            env=self.env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=20,
        )
        self.assertEqual(query.returncode, 0, query.stderr)
        self.assertEqual(json.loads(query.stdout)['id'], request_id)

    def test_04_restart_recovers_accepted_intent(self):
        atomic_json(self.tape / 'control.json', {'queryFailure': True})
        request_id = self.stage([{'path': '/b'}])
        self.assertEqual(
            self.status(request_id)['files'][0]['state'], 'SUBMITTED'
        )
        self.stop(crash=True)
        atomic_json(self.tape / 'control.json', {'failAfterAccept': True})
        self.start()
        self.wait_state(request_id, 'COMPLETED')
        atomic_json(self.tape / 'control.json', {})

    def test_05_request_scoped_release(self):
        files = [{'path': '/release-only'}]
        first, second = self.stage(files), self.stage(files)
        self.wait_state(first, 'COMPLETED')
        self.wait_state(second, 'COMPLETED')

        def released(request_id):
            status, _, body = self.request(
                'POST',
                '/api/v1/release/' + request_id,
                {'paths': ['/release-only']},
            )
            self.assertEqual(status, 200, body)
            record = (
                self.tape
                / 'backend'
                / 'requests'
                / request_id[:2]
                / request_id[2:4]
                / (request_id + '.json')
            )
            self.wait_for(
                lambda: len(json.loads(record.read_text())['operations']) == 2
            )

        released(first)
        self.assertTrue((self.tape / 'disk' / 'release-only').exists())
        released(second)
        self.wait_for(
            lambda: not (self.tape / 'disk' / 'release-only').exists()
        )
        self.assertEqual(self.status(first)['files'][0]['state'], 'COMPLETED')
        status, _, body = self.request(
            'POST', '/api/v1/archiveinfo', {'paths': ['/release-only']}
        )
        self.assertEqual(status, 200, body)
        self.assertEqual(body[0]['locality'], 'TAPE')

    def test_06_cancel_pending_and_malformed_backend_response(self):
        atomic_json(self.tape / 'control.json', {'hold': True})
        try:
            request_id = self.stage([{'path': '/a'}])
            self.wait_state(request_id, 'STARTED')
            self.assertEqual(
                self.request(
                    'POST',
                    '/api/v1/stage/' + request_id + '/cancel',
                    {'paths': ['/a']},
                )[0],
                200,
            )
            self.wait_state(request_id, 'CANCELLED')
            atomic_json(self.tape / 'control.json', {'malformedQuery': True})
            code, headers, _ = self.request(
                'POST', '/api/v1/archiveinfo', {'paths': ['/a']}
            )
            self.assertGreaterEqual(code, 500)
            self.assertEqual(
                headers['Content-Type'], 'application/problem+json'
            )
        finally:
            atomic_json(self.tape / 'control.json', {})

    def test_07_rejects_unrepresentable_paths_and_batches(self):
        cases = [
            ([{'path': path}], 400)
            for path in (
                '/a\n/b',
                '/a\r/b',
                '/a\x00b',
                '/a/../b',
                '/a/./b',
                '/a?authz=bob',
                '/a#fragment',
                'relative',
                '/',
            )
        ]
        cases += [
            ([{'path': '/a'}, {'path': '//a'}], 400),
            ([{'path': '/a'}] * 49, 413),
            ([{'path': '/a', 'diskLifetime': 1}], 400),
            ([{'path': '/a', 'targetedMetadata': []}], 400),
            ([{'path': '/a', 'targetedMetadata': {'x': 'x' * 1024}}], 413),
        ]
        for files, expected in cases:
            with self.subTest(files=files):
                status, headers, result = self.request(
                    'POST', '/api/v1/stage', {'files': files}
                )
                self.assertEqual(status, expected, result)
                self.assertEqual(
                    headers['Content-Type'], 'application/problem+json'
                )

    def test_08_max_batch_48_files_large_response(self):
        files = [
            {
                'path': f'/file_{i:02d}',
                'diskLifetime': 'PT1H',
                'targetedMetadata': {'test': {'index': i, 'desc': 'x' * 60}},
            }
            for i in range(48)
        ]
        request_id = self.stage(files)
        self.wait_state(request_id, 'COMPLETED')
        body = self.status(request_id)
        self.assertEqual(len(body['files']), 48)
        self.assertTrue(all(f['state'] == 'COMPLETED' for f in body['files']))

    def test_09_native_wire_auth_and_mutations(self):
        atomic_json(self.tape / 'control.json', {'hold': True})
        try:
            url = f'root://127.0.0.1:{self.port}'
            xrdfs = str(BUILD / 'bin' / 'xrdfs')
            # 1. Native stage with per-file CGI authz
            res = subprocess.run(
                [
                    xrdfs,
                    'prepare',
                    '-s',
                    f'{url}//a?authz=alice',
                    f'{url}//b?authz=alice',
                ],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertEqual(res.returncode, 0, res.stderr)
            request_id = res.stdout.strip()
            self.assertRegex(request_id, r'^[0-9a-f-]{36}$')
            self.wait_for(
                lambda: all(
                    f['state'] == 'STARTED'
                    for f in self.status(request_id)['files']
                )
            )

            # 2. ID-only query: wire protocol carries no paths, relying on
            # session authentication.
            q_id = subprocess.run(
                [xrdfs, url, 'query', 'prepare', request_id],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertEqual(q_id.returncode, 0, q_id.stderr)
            q_data = json.loads(q_id.stdout)
            self.assertEqual(q_data['id'], request_id)
            self.assertEqual(len(q_data['files']), 2)

            # 3. Query with paths and per-file CGI
            # Valid token succeeds
            q_ok = subprocess.run(
                [xrdfs, url, 'query', 'prepare', request_id, '/a?authz=alice'],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertEqual(q_ok.returncode, 0, q_ok.stderr)
            # Wrong token rejected (permission denied)
            q_bad = subprocess.run(
                [xrdfs, url, 'query', 'prepare', request_id, '/a?authz=bob'],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertNotEqual(q_bad.returncode, 0)
            self.assertIn('Permission denied', q_bad.stderr)

            # 4. Subset cancel with per-file CGI
            # Wrong token rejected
            c_bad = subprocess.run(
                [xrdfs, url, 'prepare', '-a', request_id, '/a?authz=bob'],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertNotEqual(c_bad.returncode, 0)
            self.assertIn('Permission denied', c_bad.stderr)

            # Valid cancel on subset /a only
            c_ok = subprocess.run(
                [xrdfs, url, 'prepare', '-a', request_id, '/a?authz=alice'],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertEqual(c_ok.returncode, 0, c_ok.stderr)
            self.wait_for(
                lambda: (
                    self.status(request_id)['files'][0]['state'] == 'CANCELLED'
                )
            )
            st = self.status(request_id)
            self.assertEqual(st['files'][0]['state'], 'CANCELLED')
            self.assertEqual(st['files'][1]['state'], 'STARTED')

            # 5. Evict (release) with per-file CGI
            # Release backend hold so /b completes
            atomic_json(self.tape / 'control.json', {})
            self.wait_for(
                lambda: (
                    self.status(request_id)['files'][1]['state'] == 'COMPLETED'
                )
            )

            # Evict with wrong token rejected
            e_bad = subprocess.run(
                [
                    xrdfs,
                    url,
                    'prepare',
                    '-e',
                    f'/b?xrd.prepare.request={request_id}&authz=bob',
                ],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertNotEqual(e_bad.returncode, 0)
            self.assertIn('Permission denied', e_bad.stderr)

            # Evict with valid token accepted
            e_ok = subprocess.run(
                [
                    xrdfs,
                    url,
                    'prepare',
                    '-e',
                    f'/b?xrd.prepare.request={request_id}&authz=alice',
                ],
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=20,
            )
            self.assertEqual(e_ok.returncode, 0, e_ok.stderr)
            bpath = (
                self.tape
                / 'backend'
                / 'requests'
                / request_id[:2]
                / request_id[2:4]
                / (request_id + '.json')
            )
            self.wait_for(
                lambda: len(json.loads(bpath.read_text())['operations']) == 3
            )
        finally:
            atomic_json(self.tape / 'control.json', {})

    def test_10_body_larger_than_protocol_buffer(self):
        # JSON whitespace makes this a valid small manifest whose transport
        # exceeds the 1 MiB HTTP buffer, without exceeding backend size limits.
        body = b' ' * (1024 * 1024 + 17) + b'{"paths":["/a"]}'
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=15)
        try:
            status, _, result = self.request(
                'POST', '/api/v1/archiveinfo', body, connection=conn, raw=True
            )
            self.assertEqual(status, 200, result)
            self.assertEqual(result[0]['path'], '/a')
            sock = conn.sock
            self.assertIsNotNone(sock)
            status, _, _ = self.request(
                'GET', '/.well-known/wlcg-tape-rest-api', connection=conn
            )
            self.assertEqual(status, 200)
            self.assertIs(conn.sock, sock)
        finally:
            conn.close()

    def test_11_expect_continue_and_fragmented_body(self):
        body = b' ' * 32768 + b'{"paths":["/a"]}'
        with socket.create_connection(('127.0.0.1', self.port), 10) as sock:
            sock.sendall(
                (
                    'POST /api/v1/archiveinfo HTTP/1.1\r\n'
                    'Host: localhost\r\nAuthorization: Bearer alice\r\n'
                    'Expect: 100-continue\r\n'
                    f'Content-Length: {len(body)}\r\n\r\n'
                ).encode()
            )
            # Do not send any body until the server acknowledges the headers.
            with sock.makefile('rb') as stream:
                self.assertEqual(
                    stream.readline(), b'HTTP/1.1 100 Continue\r\n'
                )
                self.assertEqual(stream.readline(), b'\r\n')
            sock.sendall(body[:17])
            time.sleep(0.05)
            sock.sendall(body[17:])
            response = http.client.HTTPResponse(sock)
            response.begin()
            self.assertEqual(response.status, 200, response.read())
            response.close()

    def test_12_unread_bodies_close_connection(self):
        cases = [
            ('Content-Length: 4194305\r\n', b'', 413),
            ('Transfer-Encoding: chunked\r\n', b'0\r\n\r\n', 411),
            ('Content-Length: 100\r\n', b'{', 400),
        ]
        for framing, body, expected in cases:
            with self.subTest(framing=framing):
                with socket.create_connection(
                    ('127.0.0.1', self.port), 10
                ) as sock:
                    sock.sendall(
                        (
                            'POST /api/v1/stage HTTP/1.1\r\n'
                            'Host: localhost\r\n' + framing + '\r\n'
                        ).encode()
                        + body
                    )
                    if expected == 400:
                        sock.shutdown(socket.SHUT_WR)
                    response = http.client.HTTPResponse(sock)
                    response.begin()
                    self.assertEqual(response.status, expected)
                    self.assertTrue(response.will_close)
                    self.assertEqual(
                        json.loads(response.read())['status'], expected
                    )
                    response.close()
                    self.assertEqual(sock.recv(1), b'')

    def test_13_non_post_body_and_connection_close(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        try:
            # A body on GET must be consumed, even though discovery ignores it.
            status, _, _ = self.request(
                'GET',
                '/.well-known/wlcg-tape-rest-api',
                b'not another HTTP request',
                connection=conn,
                raw=True,
            )
            self.assertEqual(status, 200)
            sock = conn.sock
            self.assertIsNotNone(sock)
            status, _, result = self.request(
                'POST',
                '/api/v1/archiveinfo',
                {'paths': ['/a']},
                connection=conn,
            )
            self.assertEqual(status, 200, result)
            self.assertIs(conn.sock, sock)
        finally:
            conn.close()
        for path in (
            '/.well-known/wlcg-tape-rest-api',
            '/api/v1/stage/00000000-0000-0000-0000-000000000000',
        ):
            with self.subTest(path=path):
                with socket.create_connection(
                    ('127.0.0.1', self.port), 10
                ) as sock:
                    sock.sendall(
                        (
                            f'GET {path} HTTP/1.1\r\nHost: localhost\r\n'
                            'Connection: close\r\n\r\n'
                        ).encode()
                    )
                    response = http.client.HTTPResponse(sock)
                    response.begin()
                    self.assertTrue(response.will_close)
                    response.read()
                    response.close()
                    self.assertEqual(sock.recv(1), b'')

    def test_14_mutation_credentials_do_not_survive_keepalive(self):
        request_id = self.stage()
        self.wait_state(request_id, 'COMPLETED')
        stage_path = '/api/v1/stage/' + request_id
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        try:
            status, _, _ = self.request('GET', stage_path, connection=conn)
            self.assertEqual(status, 200)
            sock = conn.sock
            self.assertIsNotNone(sock)
            for token in ('bob', None, 'invalid'):
                for method, path, body in (
                    ('POST', stage_path + '/cancel', {'paths': ['/a']}),
                    (
                        'POST',
                        '/api/v1/release/' + request_id,
                        {'paths': ['/a']},
                    ),
                    ('DELETE', stage_path, None),
                ):
                    with self.subTest(token=token, method=method, path=path):
                        status, _, result = self.request(
                            method, path, body, token, conn
                        )
                        self.assertEqual(status, 403, result)
                        self.assertIs(conn.sock, sock)
            status, _, _ = self.request('GET', stage_path, connection=conn)
            self.assertEqual(status, 200)
            self.assertIs(conn.sock, sock)
            status, _, _ = self.request('DELETE', stage_path, connection=conn)
            self.assertEqual(status, 200)
        finally:
            conn.close()

    def test_15_disconnect_does_not_cancel_admitted_stage(self):
        # Wait for admission on disk, then disconnect without reading the
        # response. The durable job must outlive the HTTP request and bridge.
        records = self.tape / 'registry' / 'requests'
        before = set(records.glob('*/*/*.json'))
        body = b'{"files":[{"path":"/a"}]}'
        with socket.create_connection(('127.0.0.1', self.port), 10) as sock:
            sock.sendall(
                (
                    'POST /api/v1/stage HTTP/1.1\r\nHost: localhost\r\n'
                    'Authorization: Bearer alice\r\n'
                    f'Content-Length: {len(body)}\r\n\r\n'
                ).encode()
                + body
            )
            self.wait_for(
                lambda: bool(set(records.glob('*/*/*.json')) - before)
            )
        created = set(records.glob('*/*/*.json')) - before
        self.assertEqual(len(created), 1)
        request_id = created.pop().stem
        self.wait_state(request_id, 'COMPLETED')

    def test_16_head_error_has_no_body(self):
        with socket.create_connection(('127.0.0.1', self.port), 10) as sock:
            sock.sendall(
                b'HEAD /api/v1/stage HTTP/1.1\r\nHost: localhost\r\n'
                b'Connection: close\r\n\r\n'
            )
            # Inspect all bytes through EOF, including any forbidden HEAD body.
            with sock.makefile('rb') as stream:
                response = stream.read()
            headers, body = response.split(b'\r\n\r\n', 1)
            self.assertIn(b' 405 ', headers.split(b'\r\n', 1)[0])
            self.assertEqual(body, b'')

    def test_17_legacy_handler_keepalive_close_and_failure(self):
        # No credentials and no native bridge required for legacy handlers.
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        try:
            for path in ('/legacy/simple', '/legacy/stream'):
                status, _, body = self.request(
                    'GET', path, token=None, connection=conn)
                self.assertEqual((status, body), (200, {}))
                if path.endswith('simple'):
                    sock = conn.sock
                else:
                    self.assertIs(conn.sock, sock)
            conn.request('GET', '/legacy/fail')
            response = conn.getresponse()
            with self.assertRaises(http.client.IncompleteRead):
                response.read()
        finally:
            conn.close()
        for path in ('/legacy/simple', '/legacy/stream'):
            with socket.create_connection(
                    ('127.0.0.1', self.port), 10) as sock:
                request = (f'GET {path} HTTP/1.1\r\nHost: localhost\r\n'
                           'Connection: close\r\n\r\n')
                sock.sendall(request.encode())
                with sock.makefile('rb') as stream:
                    response = stream.read()
                headers, body = response.split(b'\r\n\r\n', 1)
                self.assertIn(b' 200 ', headers.split(b'\r\n', 1)[0])
                self.assertEqual(body, b'{}')

    def test_18_real_bridge_response_limit_closes_connection(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        try:
            status, _, body = self.request(
                'GET', '/bridge-test/overflow', connection=conn)
            self.assertEqual((status, body), (502, {}))
            # Partial native replies close the bridge.
            self.assertIsNone(conn.sock)
            status, _, _ = self.request(
                'GET', '/.well-known/wlcg-tape-rest-api', connection=conn)
            self.assertEqual(status, 200)
            self.assertIsNotNone(conn.sock)
        finally:
            conn.close()

    def test_19_canonical_response_shapes(self):
        request_id = self.stage([{'path': '/a'}])
        self.wait_state(request_id, 'COMPLETED')
        status, _, body = self.request('GET', '/api/v1/stage/' + request_id)
        self.assertEqual(status, 200)
        self.assertEqual(body['id'], request_id)
        self.assertNotIn('requestId', body)
        for field in ('createdAt', 'startedAt', 'completedAt'):
            self.assertIsInstance(body[field], int)
            self.assertGreater(body[field], 0)
        self.assertEqual(len(body['files']), 1)
        file = body['files'][0]
        self.assertEqual((file['path'], file['state']), ('/a', 'COMPLETED'))
        self.assertIsInstance(file['finishedAt'], int)
        status, _, body = self.request(
            'POST', '/api/v1/archiveinfo', {'paths': ['/a']})
        self.assertEqual(status, 200)
        self.assertEqual(body, [{'path': '/a', 'locality': 'DISK_AND_TAPE'}])

    def test_20_raw_null_and_allow_headers(self):
        for path, body in (
                ('/api/v1/stage', '{"files":[{"path":"/a"}]}'),
                ('/api/v1/archiveinfo', '{"paths":["/a"]}')):
            status, _, _ = self.request(
                'POST', path, body + '\x00garbage', raw=True)
            self.assertEqual(status, 400)
            status, headers, _ = self.request('GET', path)
            self.assertEqual(status, 405)
            self.assertEqual(headers['Allow'], 'POST')


if __name__ == '__main__':
    unittest.main(verbosity=2)

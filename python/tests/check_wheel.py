"""Check a wheel through a pyproject dependency in a fresh virtual environment.

Run with pip, setuptools >= 61 and wheel in the build interpreter:
    python python/tests/check_wheel.py path/to/xrootd.whl
"""

import argparse
import http.server
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import venv


PAYLOAD = b'XRootD wheel HTTP transfer\n' * 1024
TOKEN = 'wheel-test-token'


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _headers(self, length, status=200):
        self.send_response(status)
        self.send_header('Content-Length', str(length))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Allow', 'OPTIONS, HEAD, GET')
        self.end_headers()

    def _authorized(self):
        if self.headers.get('Authorization') == 'Bearer ' + TOKEN:
            return True
        self._headers(0, 401)
        return False

    def do_OPTIONS(self):
        self._headers(0)

    def do_HEAD(self):
        if self._authorized():
            self._headers(len(PAYLOAD))

    def do_GET(self):
        if not self._authorized():
            return
        data = PAYLOAD
        requested = self.headers.get('Range')
        if requested:
            first, last = requested.removeprefix('bytes=').split('-')
            start = int(first)
            end = int(last) if last else len(data) - 1
            data = data[start:end + 1]
            self.send_response(206)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Content-Range',
                             'bytes {}-{}/{}'.format(start, end, len(PAYLOAD)))
            self.end_headers()
        else:
            self._headers(len(data))
        self.wfile.write(data)


def run(command, **kwargs):
    result = subprocess.run([str(arg) for arg in command],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=60, **kwargs)
    if result.returncode:
        raise RuntimeError('{} exited {}\n{}\n{}'.format(
            command, result.returncode, result.stdout.decode(errors='replace'),
            result.stderr.decode(errors='replace')))
    return result


def check(wheel, directory):
    # A pyproject consumer exercises dependency resolution: the wheel is never
    # explicitly installed into the test environment.
    consumer = directory / 'consumer'
    consumer.mkdir()
    (consumer / 'pyproject.toml').write_text('''
[build-system]
requires = ["setuptools>=61"]
build-backend = "setuptools.build_meta"
[project]
name = "xrootd-cli-consumer"
version = "0.0.0"
dependencies = ["xrootd"]
[tool.setuptools]
packages = []
''')
    wheelhouse = directory / 'wheelhouse'
    wheelhouse.mkdir()
    shutil.copy2(wheel, wheelhouse)
    run([sys.executable, '-m', 'pip', 'wheel', '--no-deps',
         '--no-build-isolation', '--wheel-dir', wheelhouse, consumer])
    environment = directory / 'venv'
    venv.create(environment, with_pip=True)
    binary = environment / 'bin'
    run([binary / 'python', '-m', 'pip', 'install', '--no-index',
         '--find-links', wheelhouse, 'xrootd-cli-consumer'])

    # Use virtual environment commands without inherited environment overrides.
    # Native clients still read user (passwd home) and system configuration.
    home = directory / 'home'
    home.mkdir()
    env = {'PATH': str(binary) + os.pathsep + '/usr/bin:/bin',
           'HOME': str(home), 'XRD_HTTPDISABLEX509': '1',
           # Avoid reverse-DNS lookups of the runner's network interfaces.
           'XRDNET_IDENTITY': 'localhost',
           'BEARER_TOKEN': TOKEN}
    for name in ('xrdfs', 'xrdcp', 'xrdcopy'):
        assert shutil.which(name, path=env['PATH']) == str(binary / name)
        run([binary / name, '--help'], env=env)
    result = run([binary / 'python', '-c',
                  'import os; before = os.environ.copy(); '
                  'from XRootD import client; '
                  'import pyxrootd; assert os.environ == before; '
                  'print(pyxrootd.__path__[0]); '
                  'print(client.FileSystem("root://localhost"))'], env=env)
    payload = result.stdout.splitlines()[0].decode()

    # An explicit plugin directory takes precedence over the bundled defaults.
    plugins = directory / 'plugins'
    plugins.mkdir()
    override = dict(env, XRD_PLUGINCONFDIR=str(plugins), XRD_LOGLEVEL='Debug')
    result = run([binary / 'xrdfs', '--help'], env=override)
    selected = 'Processing plug-in definitions in {}...'
    assert selected.format(plugins).encode() in result.stderr
    assert selected.format(payload).encode() not in result.stderr

    # Test stdin and native argument forwarding through both copy entry points.
    for name in ('xrdcp', 'xrdcopy'):
        output = directory / (name + '-stdin')
        run([binary / name, '-', output], input=PAYLOAD, env=env)
        assert output.read_bytes() == PAYLOAD

    certificate = directory / 'certificate.pem'
    key = directory / 'key.pem'
    run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
         '-keyout', key, '-out', certificate, '-days', '1',
         '-subj', '/CN=localhost', '-addext', 'subjectAltName=IP:127.0.0.1'])
    env['X509_CERT_FILE'] = str(certificate)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = 'https://127.0.0.1:{}/payload'.format(server.server_port)
    try:
        output = directory / 'https-copy'
        run([binary / 'xrdfs', 'stat', url], env=env)
        run([binary / 'xrdcp', url, output], env=env)
        assert output.read_bytes() == PAYLOAD
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    print('PASS: pyproject dependency, CLI paths/help/stdin, bindings, '
          'plugin override, bearer-authenticated HTTPS stat/copy')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wheel', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='xrootd-wheel-') as temporary:
        check(args.wheel.resolve(), Path(temporary))

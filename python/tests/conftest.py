import os
import shutil
import subprocess
import sys
import tempfile
import time

from pathlib import Path

import pytest

# pytest already puts this directory at the front of sys.path before importing
# conftest.py, but be explicit so the import below keeps working if the tests
# are ever run with --import-mode=importlib.

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from env import SERVER_PORT, SERVER_URL, release_reserved_port

CONFIG = Path(__file__).resolve().parent / 'xrootd.cfg'

# The remaining test modules do not talk to a server, so there is no point in
# starting one for them.

NEEDS_SERVER = {'test_copy.py', 'test_file.py', 'test_filesystem.py',
                'test_glob.py', 'test_threads.py'}

TIMEOUT = 30

# A hard-killed pytest, e.g. one cut down by a CTest timeout, never reaches
# the teardown of the fixture below and would leave the server running
# forever. The watchdog below holds the read end of a pipe from pytest, so
# the pipe closing tells it that pytest exited, however that happened. A
# byte arriving first means the teardown ran and there is nothing to do;
# end of file without one means the server is orphaned, and the watchdog
# kills it and removes its directory.

WATCHDOG = """
import os, shutil, signal, sys, time

pid, basedir = int(sys.argv[1]), sys.argv[2]

if sys.stdin.buffer.read(1):
    sys.exit(0)

try:
    os.kill(pid, signal.SIGKILL)
except OSError:
    pass

deadline = time.monotonic() + 10

while time.monotonic() < deadline:
    try:
        os.kill(pid, 0)
    except OSError:
        break
    time.sleep(0.1)

shutil.rmtree(basedir, ignore_errors=True)
"""


@pytest.fixture(scope='session', autouse=True)
def xrootd_server(request):
    """Run an XRootD server for the duration of the test session."""

    collected = {os.path.basename(str(getattr(item, 'path', None) or item.fspath))
                 for item in request.session.items}

    if not collected & NEEDS_SERVER:
        yield None
        return

    xrootd = os.environ.get('XROOTD') or shutil.which('xrootd')

    if xrootd is None:
        pytest.skip('cannot find the xrootd server binary, set $XROOTD')

    # The server runs with its working directory set to the temporary
    # directory below, so a relative path would not resolve there.

    xrootd = os.path.abspath(xrootd)

    # The readiness check below queries the server with xrdfs, which usually
    # sits next to the server binary.

    xrdfs = (os.environ.get('XRDFS')
             or shutil.which('xrdfs', path=os.path.dirname(xrootd))
             or shutil.which('xrdfs'))

    if xrdfs is None:
        pytest.skip('cannot find the xrdfs binary, set $XRDFS')

    xrdfs = os.path.abspath(xrdfs)

    # A short path directly under /tmp is required here: the server appends
    # <instance>/.xrd/admin to it to create its admin socket, and the path of
    # a unix socket must stay under 104 characters on macOS, less than the
    # pytest temporary directory alone needs there. The directory is removed
    # in the teardown below, so that nothing is left behind under /tmp.

    basedir = Path(tempfile.mkdtemp(prefix='pyxrootd-', dir='/tmp'))

    # Directories are created on demand when opening a file for writing, but
    # not by locate(), dirlist() or truncate(), which some tests call first.

    (basedir / 'data' / 'tmp').mkdir(parents=True)

    logfile = basedir / 'xrootd.log'

    # Restrict the server to IPv4, matching the loopback address used by the
    # clients, see env.py. This must not be left to chance on either side.

    command = [xrootd, '-c', str(CONFIG), '-n', 'pyxrootd',
               '-p', str(SERVER_PORT), '-I', 'v4']

    if os.environ.get('PYXROOTD_TEST_DEBUG'):
        command.append('-d')

    environ = dict(os.environ, PYXROOTD_TEST_BASEDIR=str(basedir))

    release_reserved_port()

    try:
        with open(logfile, 'wb') as log:
            # Run in the foreground and stay in the same process group, so
            # that a timeout kill from CTest reaches the server as well.
            server = subprocess.Popen(command, cwd=str(basedir), env=environ,
                                      stdout=log, stderr=subprocess.STDOUT)
    except Exception:
        shutil.rmtree(basedir, ignore_errors=True)
        raise

    try:
        watchdog = subprocess.Popen([sys.executable, '-c', WATCHDOG,
                                     str(server.pid), str(basedir)],
                                    stdin=subprocess.PIPE)
    except Exception:
        server.kill()
        server.wait()
        shutil.rmtree(basedir, ignore_errors=True)
        raise

    try:
        wait_until_ready(server, xrdfs, logfile)
        yield SERVER_URL
    finally:
        server.terminate()

        try:
            server.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()

        if request.session.testsfailed:
            sys.stderr.write(read(logfile))

        shutil.rmtree(basedir, ignore_errors=True)

        # Stand the watchdog down: the byte says the teardown above ran.

        try:
            watchdog.communicate(b'.', timeout=10)
        except Exception:
            watchdog.kill()
            watchdog.wait()


def wait_until_ready(server, xrdfs, logfile):
    """Wait until the server answers a query, like the shell tests do.

    Using a fresh xrdfs process for each attempt matters: a failed request on
    an in-process client would leave its connection in an error state which
    outlives the deadline used here, turning one slow start into a failure.
    """

    deadline = time.monotonic() + TIMEOUT
    output = b''

    # Give up on a connection quickly: with the default settings, a query
    # started before the server listens waits out a long connection window
    # instead of failing, which would make every startup take seconds. Short
    # timeouts are safe precisely because each attempt is a fresh process.

    environ = dict(os.environ, XRD_CONNECTIONWINDOW='3', XRD_CONNECTIONRETRY='2',
                   XRD_REQUESTTIMEOUT='5', XRD_TIMEOUTRESOLUTION='1')

    # Give the server a moment to bind its port, so that the first query
    # usually succeeds at once instead of waiting out a connection window.

    time.sleep(0.2)

    while time.monotonic() < deadline:
        check_alive(server, logfile)

        try:
            result = subprocess.run([xrdfs, SERVER_URL, 'query', 'config',
                                     'version'], stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, env=environ,
                                    timeout=15)

            if result.returncode == 0:
                return

            output = result.stdout
        except subprocess.TimeoutExpired as error:
            output = error.stdout or b''

        time.sleep(0.5)

    fail(server, logfile, 'did not answer a version query within %ds: %s'
         % (TIMEOUT, output.decode(errors='replace').strip()))


def check_alive(server, logfile):
    if server.poll() is not None:
        fail(server, logfile, 'exited with status %d' % server.returncode)


def fail(server, logfile, message):
    if server.poll() is None:
        server.kill()
        server.wait()

    pytest.fail('XRootD server %s\n\n%s' % (message, read(logfile)))


def read(logfile):
    try:
        return logfile.read_text(errors='replace')
    except OSError as error:
        return 'could not read %s: %s' % (logfile, error)

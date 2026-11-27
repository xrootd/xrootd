# The client stops its threads in an exit handler of the C library. For a
# Python program, this handler runs after the finalization of the interpreter.
# A thread of the client which calls a Python callback after the finalization
# then waits forever for the global interpreter lock, and the exit handler
# waits forever for the thread. Thus, the Python bindings stop the threads of
# the client in an atexit handler of Python, before the finalization. This
# handler also closes the files which are still open.
#
# Each test runs a program which exits while the client still has work, and
# checks that the program exits with status 0. One exit hits a race only
# sometimes, thus the tests of races run many programs in parallel.

import subprocess
import sys
import textwrap
import time

import pytest

from env import SERVER_URL
from XRootD import client
from XRootD.client.flags import StatInfoFlags

PROGRAMS = 16
TIMEOUT = 30

# Before its first login, the client looks up the names of the addresses of
# this host. On macOS, a framework build of Python runs as the app
# org.python.python, and an app needs the permission of the user for the local
# network. Until the user allows or denies it, macOS holds each lookup which
# can use multicast DNS, for example each reverse lookup, for 35 s. Then the
# login of each program takes longer than TIMEOUT, for example on a GitHub
# runner, which has no user. Thus, a program first measures the login, and
# the tests are skipped when it takes longer than LOGIN_TIMEOUT. The program
# prints a line before its exit, thus a program which does not exit does not
# cause a skip.

LOGIN_TIMEOUT = 10


@pytest.fixture(scope='module', autouse=True)
def fast_login():
    if sys.platform != 'darwin':
        return

    probe = subprocess.Popen([sys.executable, '-c', textwrap.dedent('''
        import sys
        from XRootD import client

        client.FileSystem(sys.argv[1]).stat('/tmp')
        print('logged in', flush=True)
    '''), SERVER_URL], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    try:
        stdout, stderr = probe.communicate(timeout=LOGIN_TIMEOUT)
    except subprocess.TimeoutExpired:
        probe.kill()
        stdout, stderr = probe.communicate()
        if not stdout.startswith(b'logged in'):
            pytest.skip('the login takes longer than %d s: allow the local '
                        'network access of Python' % LOGIN_TIMEOUT)

    assert stdout.startswith(b'logged in'), stderr.decode(errors='replace')


def run_programs(code):
    """Run PROGRAMS copies of code, and return the exit status of each.

    Each program gets the URL of the server and its own index as arguments.
    A program which does not exit within TIMEOUT seconds after the start is
    killed, and then has the exit status -9. The second value of the result
    tells the defect of each program which failed.
    """

    code = textwrap.dedent(code)
    processes = [subprocess.Popen([sys.executable, '-c', code, SERVER_URL,
                                   str(n)], stdout=subprocess.DEVNULL,
                                  stderr=subprocess.PIPE)
                 for n in range(PROGRAMS)]

    deadline = time.monotonic() + TIMEOUT
    status = []
    errors = []

    for n, process in enumerate(processes):
        try:
            timeout = max(0, deadline - time.monotonic())
            _, stderr = process.communicate(timeout=timeout)
            error = 'program %d: exit status %d' % (n, process.returncode)
        except subprocess.TimeoutExpired:
            process.kill()
            _, stderr = process.communicate()
            error = 'program %d: no exit within %d s' % (n, TIMEOUT)

        status.append(process.returncode)

        if process.returncode != 0:
            errors.append(error + '\n' + stderr.decode(errors='replace'))

    return status, '\n'.join(errors)


def run_program(code, *args):
    """Run code with the arguments args, and return the completed process."""

    return subprocess.run([sys.executable, '-c', textwrap.dedent(code)]
                          + list(args),
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                          timeout=TIMEOUT)


def test_exit_with_pending_callbacks():
    status, errors = run_programs('''
        import sys
        from XRootD import client

        def callback(status, response, hostlist):
            pass

        fs = client.FileSystem(sys.argv[1])

        for _ in range(100):
            fs.stat('/tmp', callback=callback)
    ''')

    assert status == [0] * PROGRAMS, errors


def test_exit_during_a_callback():
    # The callback releases the global interpreter lock in the sleep, and the
    # program exits while the worker thread of the client is in it.

    status, errors = run_programs('''
        import sys
        import threading
        import time
        from XRootD import client

        started = threading.Event()

        def callback(status, response, hostlist):
            started.set()
            time.sleep(0.5)

        fs = client.FileSystem(sys.argv[1])
        fs.stat('/tmp', callback=callback)
        assert started.wait(10), 'the callback did not start'
    ''')

    assert status == [0] * PROGRAMS, errors


def test_exit_with_open_files():
    status, errors = run_programs('''
        import sys
        from XRootD import client
        from XRootD.client.flags import OpenFlags

        files = []

        for i in range(10):
            f = client.File()
            f.open(sys.argv[1] + '/tmp/exit-%s-%d' % (sys.argv[2], i),
                   OpenFlags.DELETE)
            f.write(b'data')
            files.append(f)
    ''')

    assert status == [0] * PROGRAMS, errors


def test_exit_during_a_request_of_a_daemon_thread():
    # The daemon thread waits for a response without the global interpreter
    # lock when the program exits.

    status, errors = run_programs('''
        import sys
        import threading
        import time
        from XRootD import client

        fs = client.FileSystem(sys.argv[1])

        def stats():
            while True:
                fs.stat('/tmp')

        threading.Thread(target=stats, daemon=True).start()
        time.sleep(0.2)
    ''')

    assert status == [0] * PROGRAMS, errors


def test_exit_closes_an_open_posc_file():
    # The server removes a file which was opened with OpenFlags.POSC (persist
    # on successful close) when the client disconnects without a close.

    program = run_program('''
        import sys
        from XRootD import client
        from XRootD.client.flags import OpenFlags

        f = client.File()
        status, _ = f.open(sys.argv[1], OpenFlags.DELETE | OpenFlags.POSC)
        assert status.ok, status.message
        status, _ = f.write(b'data')
        assert status.ok, status.message
    ''', SERVER_URL + '/tmp/exit-posc')

    assert program.returncode == 0, program.stderr.decode(errors='replace')

    status, info = client.FileSystem(SERVER_URL).stat('/tmp/exit-posc')

    assert status.ok, status.message
    assert info.size == 4
    assert not info.flags & StatInfoFlags.POSC_PENDING


def test_exit_closes_an_open_posc_file_of_a_copy():
    # copy.copy() makes a File without a call of __init__. The copy shares
    # the open file, and only the copy is left at exit.

    program = run_program('''
        import copy
        import gc
        import sys
        from XRootD import client
        from XRootD.client.flags import OpenFlags

        f = client.File()
        status, _ = f.open(sys.argv[1], OpenFlags.DELETE | OpenFlags.POSC)
        assert status.ok, status.message
        g = copy.copy(f)
        del f
        gc.collect()
        status, _ = g.write(b'data')
        assert status.ok, status.message
    ''', SERVER_URL + '/tmp/exit-posc-copy')

    assert program.returncode == 0, program.stderr.decode(errors='replace')

    status, info = client.FileSystem(SERVER_URL).stat('/tmp/exit-posc-copy')

    assert status.ok, status.message
    assert info.size == 4
    assert not info.flags & StatInfoFlags.POSC_PENDING


def test_exit_closes_an_open_posc_file_of_a_copy_of_a_subclass():
    # copy.copy() gives the arguments of __getnewargs__ to __new__, and copies
    # the slots. The copy of the subclass must keep them, and only the copy is
    # left at exit.

    program = run_program('''
        import copy
        import gc
        import sys
        from XRootD import client
        from XRootD.client.flags import OpenFlags

        class TaggedFile(client.File):
            __slots__ = ('tag',)

            def __new__(cls, token):
                self = super(TaggedFile, cls).__new__(cls)
                self.token = token
                return self

            def __init__(self, token):
                super(TaggedFile, self).__init__()

            def __getnewargs__(self):
                return (self.token,)

        f = TaggedFile('token')
        f.tag = 'tag'
        status, _ = f.open(sys.argv[1], OpenFlags.DELETE | OpenFlags.POSC)
        assert status.ok, status.message
        g = copy.copy(f)
        assert (g.token, g.tag) == ('token', 'tag'), (g.token, g.tag)
        del f
        gc.collect()
        status, _ = g.write(b'data')
        assert status.ok, status.message
    ''', SERVER_URL + '/tmp/exit-posc-copy-subclass')

    assert program.returncode == 0, program.stderr.decode(errors='replace')

    status, info = client.FileSystem(SERVER_URL).stat(
        '/tmp/exit-posc-copy-subclass')

    assert status.ok, status.message
    assert info.size == 4
    assert not info.flags & StatInfoFlags.POSC_PENDING


def test_exit_does_not_read_attributes_of_other_objects():
    # isinstance() reads the attribute __class__ of an object of another
    # class. A package can run its own code for this attribute, for example
    # to show a warning (#2910).

    program = run_program('''
        import os
        import sys
        from XRootD import client

        class Trap(object):
            def __getattribute__(self, name):
                if name == '__class__':
                    sys.stderr.write('the exit handler read Trap.__class__')
                    sys.stderr.flush()
                    os._exit(3)
                return object.__getattribute__(self, name)

        trap = Trap()
        f = client.File()
    ''')

    assert program.returncode == 0, program.stderr.decode(errors='replace')

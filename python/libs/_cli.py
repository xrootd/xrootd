"""Launch the native clients shipped alongside the Python bindings."""

import os
import sys


def _payload_dir():
    return os.path.join(os.path.dirname(os.path.dirname(__file__)), 'pyxrootd')


def _exec(command):
    # Resolve from this installation, never from PATH, so all clients use the
    # matching bundled libraries. exec preserves signals and native exit codes.
    directory = _payload_dir()
    env = os.environ.copy()
    if os.path.isfile(os.path.join(directory, 'http.conf')):
        env.setdefault('XRD_PLUGINCONFDIR', directory)
    executable = os.path.join(directory, command)
    os.execve(executable, [command] + sys.argv[1:], env)


def xrdfs():
    _exec('xrdfs')


def xrdcp():
    _exec('xrdcp')


def xrdcopy():
    _exec('xrdcp')

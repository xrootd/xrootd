import os
import socket

__all__ = ['SERVER_PORT', 'SERVER_URL', 'smallfile', 'smallcopy',
           'smallbuffer', 'bigfile', 'bigcopy', 'release_reserved_port']

_reserved = None


def _reserve_port():
    """Reserve an unused port for the test server.

    The socket is kept listening until conftest.py releases it just before
    starting the server, so that no other process can take the port in the
    meantime. A socket which is bound but not listening does not exclude
    another binder using SO_REUSEADDR, while a listening socket which never
    accepts anything leaves no connection in TIME_WAIT when it is closed.
    """
    global _reserved
    _reserved = socket.socket()
    _reserved.bind(('127.0.0.1', 0))
    _reserved.listen(1)
    return _reserved.getsockname()[1]


def release_reserved_port():
    global _reserved
    if _reserved is not None:
        _reserved.close()
        _reserved = None


# Use the IPv4 loopback address literally rather than localhost: an address
# needs no name resolution, which cannot be relied upon in every test
# environment, and pinning the address family matters because the client
# prefers IPv6 and retries a failed connection within a window much longer
# than the server startup timeout, so it cannot be left to discover by
# falling back that the server answers on IPv4, as on hosts where localhost
# resolves to ::1 first. The server is restricted to IPv4 by conftest.py.

SERVER_PORT = int(os.environ.get('PYXROOTD_TEST_PORT') or _reserve_port())
SERVER_URL  = 'root://127.0.0.1:%d/' % SERVER_PORT
smallfile   = SERVER_URL + '/tmp/spam'
smallcopy   = SERVER_URL + '/tmp/eggs'
smallbuffer = 'gre\0en\neggs\nand\nham\n'
bigfile     = SERVER_URL + '/tmp/bigfile'
bigcopy     = SERVER_URL + '/tmp/bigcopy'

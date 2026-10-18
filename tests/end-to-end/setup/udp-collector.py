#!/usr/bin/env python3
"""Write each UDP datagram which xrootd sends to a monitoring collector as one
line of a file, for the tests of the monitoring (see start_collector in
server.bash).

Usage: udp-collector.py <portfile> <outfile> <pidfile>

Binds a free port on both IP stacks, writes it to <portfile>, then detaches
into its own session, thus a test can start a server which sends to the port
as soon as this command returns. With 'send json' a datagram is JSON, the last
record terminated by a null byte."""

import os
import socket
import sys

portfile, outfile, pidfile = sys.argv[1:4]

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
sock.bind(("::", 0))

with open(portfile, "w") as f:
    f.write("%d\n" % sock.getsockname()[1])

if os.fork() != 0:
    os._exit(0)

os.setsid()

# Without the descriptors of the caller, which BATS reads until all holders
# close them: 0 to 2, and others like 3 and 4, on which BATS reads the output
# of the test. Keep only the socket.
devnull = os.open(os.devnull, os.O_RDWR)
for fd in (0, 1, 2):
    os.dup2(devnull, fd)
maxfd = min(os.sysconf("SC_OPEN_MAX"), 65536)
os.closerange(3, sock.fileno())
os.closerange(sock.fileno() + 1, maxfd)

with open(pidfile, "w") as f:
    f.write("%d\n" % os.getpid())

with open(outfile, "a", buffering=1) as out:
    while True:
        data, _ = sock.recvfrom(65536)
        text = data.rstrip(b"\x00").decode("utf-8", "replace")
        out.write(text.rstrip("\n").replace("\n", " ") + "\n")

#!/usr/bin/env python3
"""Append xrootd 'g' monitoring stream datagrams to a file, one record per line.

Used by cache.sh to read the pfc g-stream, which is the only place b_todisk and
b_prefetch are published -- the cinfo does not carry them.

With 'send json' the datagram is JSON all the way through: an envelope record
followed by newline-separated event records, the last one null-terminated. That
is unlike the binary 'dflthdr' form, which puts an XrdXrootdMonHeader and an
XrdXrootdMonGS in front and would need unpacking here.

Detaches into its own session and writes its pid to <pidfile>, which test.sh
picks up for teardown. Without the detach ctest kills it along with the rest of
the setup test's process group, and nothing is left listening. The socket is
bound before forking, so once this command returns the port is already up and
the caller can start the server without racing it.

Usage: gstream_recv.py <port> <outfile> <pidfile>
"""

import os
import socket
import sys

port, outfile, pidfile = sys.argv[1], sys.argv[2], sys.argv[3]

# Bind dual-stack: the config names the destination as localhost, which
# resolves to ::1 here and to 127.0.0.1 elsewhere.
sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
sock.bind(("::", int(port)))

if os.fork() != 0:
    os._exit(0)

os.setsid()

# Detach from the caller's stdin/stdout/stderr as well as from its session:
# ctest reads a test's output pipe until every holder closes it, so merely
# surviving the process group is not enough -- holding the inherited fd would
# hang the run long after the test script itself has finished.
devnull = os.open(os.devnull, os.O_RDWR)
for fd in (0, 1, 2):
    os.dup2(devnull, fd)
if devnull > 2:
    os.close(devnull)

with open(pidfile, "w") as f:
    f.write("%d\n" % os.getpid())

with open(outfile, "a", buffering=1) as out:
    while True:
        data, _ = sock.recvfrom(65536)
        text = data.rstrip(b"\x00").decode("utf-8", "replace")
        out.write(text.rstrip("\n") + "\n")

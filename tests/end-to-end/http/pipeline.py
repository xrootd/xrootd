#!/usr/bin/env python3

# Send the requests of stdin to the server in one write (HTTP/1.1 pipelining),
# and print the status code of each response, one per line. The responses
# must have a Content-Length, or no body.

import socket
import sys


def read_response(sock, pending):
    """Return the status code of the next response, and the bytes after it."""
    while b"\r\n\r\n" not in pending:
        chunk = sock.recv(65536)
        if not chunk:
            return None, b""
        pending += chunk

    head, pending = pending.split(b"\r\n\r\n", 1)
    lines = head.split(b"\r\n")
    length = 0
    for line in lines[1:]:
        name, _, value = line.partition(b":")
        if name.strip().lower() == b"content-length":
            length = int(value)

    while len(pending) < length:
        chunk = sock.recv(65536)
        if not chunk:
            break
        pending += chunk

    return lines[0].split()[1].decode("ascii"), pending[length:]


def main():
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} HOST PORT COUNT", file=sys.stderr)
        return 2

    pending = b""
    try:
        with socket.create_connection((sys.argv[1], int(sys.argv[2])), timeout=10) as sock:
            sock.sendall(sys.stdin.buffer.read())
            for _ in range(int(sys.argv[3])):
                status, pending = read_response(sock, pending)
                if status is None:
                    print("closed")
                    break
                print(status)
    except OSError as exc:
        print(f"failed: {exc}")

    return 0


if __name__ == "__main__":
    sys.exit(main())

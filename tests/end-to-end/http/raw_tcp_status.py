#!/usr/bin/env python3

import socket
import sys


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} HOST PORT", file=sys.stderr)
        return 2

    host = sys.argv[1]
    port = int(sys.argv[2])
    payload = sys.stdin.buffer.read()
    response = bytearray()
    timed_out = False
    closed = False

    try:
        with socket.create_connection((host, port), timeout=10) as sock:
            sock.settimeout(10)
            sock.sendall(payload)
            while b"\n" not in response:
                chunk = sock.recv(4096)
                if not chunk:
                    closed = True
                    break
                response.extend(chunk)
    except TimeoutError:
        timed_out = True
    except OSError as exc:
        print(f"<TCP exchange failed: {exc}>", end="")
        return 0

    if response:
        line = bytes(response).split(b"\n", 1)[0].replace(b"\r", b"")
        print(line.decode("iso-8859-1", errors="replace"), end="")
    elif timed_out:
        print("<request stalled - read timed out waiting for a status line>", end="")
    elif closed:
        print("<server closed the connection without sending a status line>", end="")
    else:
        print("<TCP exchange failed without a status line>", end="")

    return 0


if __name__ == "__main__":
    sys.exit(main())

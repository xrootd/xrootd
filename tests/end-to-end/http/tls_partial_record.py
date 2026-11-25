#!/usr/bin/env python3

# Make a TLS connection, send all but the last 10 bytes of one TLS record,
# and print when the server closes the connection.

import socket
import ssl
import sys
import time


def main():
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} HOST PORT TIMEOUT", file=sys.stderr)
        return 2

    host, port, timeout = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE

    with socket.create_connection((host, port), timeout=timeout) as sock:
        incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
        tls = context.wrap_bio(incoming, outgoing, server_hostname=host)
        while True:
            try:
                tls.do_handshake()
                break
            except ssl.SSLWantReadError:
                sock.sendall(outgoing.read())
                data = sock.recv(65536)
                if not data:
                    print("closed during the handshake")
                    return 1
                incoming.write(data)
        sock.sendall(outgoing.read())

        tls.write(f"GET /file HTTP/1.1\r\nHost: {host}\r\n\r\n".encode())
        record = outgoing.read()
        sock.sendall(record[:-10])
        start = time.monotonic()
        try:
            while sock.recv(65536):
                pass
            print(f"closed after {time.monotonic() - start:.1f} s")
        except socket.timeout:
            print(f"still open after {time.monotonic() - start:.1f} s")
        except ConnectionResetError:
            print(f"closed after {time.monotonic() - start:.1f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())

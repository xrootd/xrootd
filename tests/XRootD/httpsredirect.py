#!/usr/bin/env python3

import socket
import ssl
import sys


TIMEOUT = 5


def fetch_redirect(port, cafile):
    context = ssl.create_default_context(cafile=cafile)
    address = ("localhost", port)
    with socket.create_connection(address, timeout=TIMEOUT) as sock:
        with context.wrap_socket(sock, server_hostname="localhost") as tls:
            tls.settimeout(TIMEOUT)
            tls.sendall(
                b"GET /test HTTP/1.1\r\n"
                b"Host: localhost\r\n"
                b"Connection: Keep-Alive\r\n\r\n"
            )
            response = bytearray()
            while True:
                chunk = tls.recv(4096)
                if not chunk:
                    return response
                response.extend(chunk)


if __name__ == "__main__":
    sys.stdout.buffer.write(fetch_redirect(int(sys.argv[1]), sys.argv[2]))

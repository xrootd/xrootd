#!/usr/bin/env python3
# Minimal OTLP/HTTP receiver for the xrdmoncollect end-to-end test. It accepts
# POST requests on 127.0.0.1:<port> and, for each, appends two lines to
# <outfile>: "authz <path> <Authorization header>\n" and "<path> <body>\n";
# then replies 200. Used to assert that xrdmoncollect exports OTLP logs
# (/v1/logs) and traces (/v1/traces) and sends the bearer token.
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


def main():
    port = int(sys.argv[1])
    outfile = sys.argv[2]

    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            authz = self.headers.get("Authorization", "")
            with open(outfile, "ab") as f:
                f.write(b"authz " + self.path.encode() + b" "
                        + authz.encode() + b"\n")
                f.write(self.path.encode() + b" " + body + b"\n")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", "2")
            self.end_headers()
            self.wfile.write(b"{}")

        def log_message(self, *args):
            pass

    HTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3

"""Deterministic HTTPS token issuer used by the XRootD macaroon tests."""

import argparse
import json
import ssl
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    """Threaded HTTP server compatible with Python 3.6 and newer."""

    daemon_threads = True


SCI_BODY = "grant_type=client_credentials"


class TokenIssuerServer(ThreadingHTTPServer):
    """HTTP server carrying the trace location and advertised endpoint URL."""

    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address, handler, trace_file, advertised_url):
        super().__init__(address, handler)
        self.trace_file = trace_file
        self.advertised_url = advertised_url
        self.trace_lock = threading.Lock()

    def trace(self, entry):
        with self.trace_lock:
            with open(self.trace_file, "a", encoding="utf-8") as stream:
                json.dump(entry, stream, separators=(",", ":"))
                stream.write("\n")


class TokenIssuerHandler(BaseHTTPRequestHandler):
    """Serve fixed issuer scenarios and record every non-health request."""

    protocol_version = "HTTP/1.1"

    def log_message(self, format_string, *args):
        message = format_string % args
        print("{} - {}".format(self.address_string(), message), flush=True)

    def send_json(self, status, payload):
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ssl.SSLEOFError):
            pass

    def read_body(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        return self.rfile.read(length).decode("utf-8", errors="replace")

    def record(self, body=""):
        host = self.headers.get("Host", "")
        self.server.trace(
            {
                "method": self.command,
                "url": "https://{}{}".format(host, self.path),
                "content_type": self.headers.get("Content-Type", ""),
                "accept": self.headers.get("Accept", ""),
                "body": body,
            }
        )

    def token_endpoint(self, scenario):
        return "{}/token/{}".format(self.server.advertised_url, scenario)

    def do_GET(self):
        if self.path == "/healthz":
            self.send_json(200, {"status": "ok"})
            return

        self.record()
        metadata_prefix = "/.well-known/oauth-authorization-server/"
        if self.path.startswith(metadata_prefix):
            scenario = self.path[len(metadata_prefix):]
            if scenario == "slow-deadline":
                time.sleep(0.75)
                self.send_json(404, {"error": "metadata unavailable"})
                return
            if scenario in ("sci-success", "oauth-success", "oauth-failure"):
                self.send_json(
                    200, {"token_endpoint": self.token_endpoint(scenario)}
                )
                return
            self.send_json(404, {"error": "metadata unavailable"})
            return

        if self.path == "/openid-success/.well-known/openid-configuration":
            self.send_json(
                200, {"token_endpoint": self.token_endpoint("openid-success")}
            )
            return

        self.send_json(404, {"error": "openid metadata unavailable"})

    def do_POST(self):
        body = self.read_body()
        self.record(body)

        if self.path == "/token/sci-success":
            if body == SCI_BODY:
                self.send_json(200, {"access_token": "sci-token"})
            else:
                self.send_json(400, {"error": "unexpected SciTokens body"})
            return

        if self.path == "/token/oauth-success":
            if body == SCI_BODY:
                self.send_json(400, {"error": "OAuth scope required"})
            elif "&scopes=" in body:
                self.send_json(200, {"access_token": "oauth-token"})
            else:
                self.send_json(400, {"error": "unexpected OAuth body"})
            return

        if self.path == "/token/openid-success":
            if "&scopes=" in body:
                self.send_json(200, {"access_token": "openid-token"})
            else:
                self.send_json(400, {"error": "unexpected OAuth body"})
            return

        if self.path == "/token/oauth-failure":
            if body == SCI_BODY:
                self.send_json(400, {"error": "OAuth scope required"})
            else:
                self.send_json(403, {"error": "OAuth request rejected"})
            return

        if self.path.startswith("/storage/direct-object"):
            self.send_json(200, {"macaroon": "direct-token"})
            return

        if self.path.startswith("/storage/must-not-be-called"):
            self.send_json(200, {"macaroon": "unexpected-direct-token"})
            return

        if self.path.startswith("/storage/deadline-object"):
            self.send_json(200, {"macaroon": "unexpected-deadline-token"})
            return

        self.send_json(404, {"error": "unknown token endpoint"})


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", required=True, type=int)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--trace-file", required=True)
    return parser.parse_args()


def main():
    args = parse_arguments()
    advertised_url = "https://localhost:{}".format(args.port)
    server = TokenIssuerServer(
        (args.bind, args.port),
        TokenIssuerHandler,
        args.trace_file,
        advertised_url,
    )
    tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls_context.load_cert_chain(args.cert, args.key)
    server.socket = tls_context.wrap_socket(server.socket, server_side=True)
    server.serve_forever()


if __name__ == "__main__":
    main()

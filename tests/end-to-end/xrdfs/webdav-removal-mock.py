#!/usr/bin/env python3

import http.server
import pathlib
import socketserver
import sys
import urllib.parse


WEBDAV_LAST_MODIFIED = "Sun, 06 Nov 1994 08:49:37 GMT"
HEAD_LAST_MODIFIED = "Wed, 21 Oct 2015 07:28:00 GMT"


def resource_response(path, directory):
    resource_type = "<D:collection/>" if directory else ""
    size = "" if directory else "<D:getcontentlength>4</D:getcontentlength>"
    suffix = "/" if directory and not path.endswith("/") else ""
    return f"""
  <D:response>
    <D:href>{path}{suffix}</D:href>
    <D:propstat>
      <D:prop>
        <D:resourcetype>{resource_type}</D:resourcetype>
        {size}
        <D:getlastmodified>{WEBDAV_LAST_MODIFIED}</D:getlastmodified>
      </D:prop>
      <D:status>HTTP/1.1 200 OK</D:status>
    </D:propstat>
  </D:response>"""


class WebDAVHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, _format, *args):
        pass

    def record(self):
        # BaseHTTPRequestHandler normalizes a leading // before exposing
        # self.path. Inspect the original request target so tests catch invalid
        # WebDAV URLs such as https://host//path.
        request_target = self.requestline.split(" ", 2)[1]
        request = urllib.parse.urlsplit(request_target)
        path = request.path
        with self.server.log_path.open("a", encoding="utf-8") as stream:
            stream.write(f"{self.command} {path} {request.query}\n")
        return path

    def respond(self, status, body=b"", content_type=None):
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        if content_type:
            self.send_header("Content-Type", content_type)
        self.end_headers()
        if body:
            self.wfile.write(body)
        self.close_connection = True

    def do_OPTIONS(self):
        self.record()
        self.send_response(200)
        allow = "HEAD" if self.server.head_only else "PROPFIND"
        self.send_header("Allow", allow)
        self.send_header("Content-Length", "0")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def do_HEAD(self):
        path = self.record()
        if path != "/head-file":
            self.respond(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", "4")
        self.send_header("Last-Modified", HEAD_LAST_MODIFIED)
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def do_GET(self):
        path = self.record()
        if path != "/file":
            self.respond(404)
            return

        byte_range = self.headers.get("Range")
        if byte_range and byte_range != "bytes=0-3":
            self.respond(400)
            return

        body = b"data"
        self.send_response(206 if byte_range else 200)
        self.send_header("Content-Length", str(len(body)))
        if byte_range:
            self.send_header("Content-Range", "bytes 0-3/4")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def do_PROPFIND(self):
        path = self.record()
        directories = {
            "/directory",
            "/dry-run-tree",
            "/dry-run-tree/nested",
            "/empty",
            "/json-directory",
            "/nonempty",
        }
        files = {
            "/dry-run-file",
            "/dry-run-tree/file",
            "/dry-run-tree/nested/leaf",
            "/file",
            "/file-a",
            "/json-file",
        }
        if path not in directories and path not in files:
            self.respond(404)
            return

        responses = [resource_response(path, path in directories)]
        if path == "/nonempty" and self.headers.get("Depth") == "1":
            responses.append(resource_response("/nonempty/child", False))
        if (
            path == "/json-directory"
            and self.headers.get("Depth") == "1"
        ):
            responses.append(
                resource_response("/json-directory/child", False)
            )
        if path == "/dry-run-tree" and self.headers.get("Depth") == "1":
            responses.append(resource_response("/dry-run-tree/file", False))
            responses.append(
                resource_response("/dry-run-tree/nested", True)
            )
        if (
            path == "/dry-run-tree/nested"
            and self.headers.get("Depth") == "1"
        ):
            responses.append(
                resource_response("/dry-run-tree/nested/leaf", False)
            )
        body = (
            '<?xml version="1.0" encoding="utf-8"?>\n'
            '<D:multistatus xmlns:D="DAV:">'
            + "".join(responses)
            + "\n</D:multistatus>\n"
        ).encode("utf-8")
        self.respond(207, body, "application/xml; charset=utf-8")

    def do_DELETE(self):
        path = self.record()
        if path == "/partial":
            self.respond(207, b"<multistatus/>", "application/xml")
            return
        if path == "/forbidden":
            self.respond(403)
            return
        self.respond(204)


class WebDAVServer(http.server.HTTPServer):
    def server_bind(self):
        # HTTPServer resolves the bound address to populate server_name. That
        # lookup can block on macOS CI and is not useful to this local mock.
        socketserver.TCPServer.server_bind(self)
        self.server_name = "localhost"
        self.server_port = self.server_address[1]


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(
            "usage: webdav-removal-mock.py PORT_FILE LOG_FILE [head]"
        )

    port_path = pathlib.Path(sys.argv[1])
    log_path = pathlib.Path(sys.argv[2])
    log_path.touch()

    server = WebDAVServer(("127.0.0.1", 0), WebDAVHandler)
    server.log_path = log_path
    server.head_only = len(sys.argv) == 4 and sys.argv[3] == "head"
    port_path.write_text(str(server.server_port), encoding="ascii")
    server.serve_forever()


if __name__ == "__main__":
    main()

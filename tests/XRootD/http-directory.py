#!/usr/bin/env python3
"""Exercise xrdcp directory probes and recursion with strict WebDAV."""

import http.server
import pathlib
import posixpath
import socketserver
import subprocess
import sys
import tempfile
import threading
import urllib.parse
import xml.sax.saxutils


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    files = {
        "/src/tree/good.bin": b"good data\n",
        "/src/tree/nested/leaf.bin": b"nested data\n",
        "/src/partial/good.bin": b"partial data\n",
    }
    uploads = {}
    directories = {"/dst"}
    seen = []

    def log_message(self, *args):
        pass

    def reply(self, code, body=b"", headers=None):
        self.send_response(code)
        if "Content-Length" not in (headers or {}):
            self.send_header("Content-Length", str(len(body)))
        for key, value in (headers or {}).items():
            self.send_header(key, value)
        self.end_headers()
        if self.command not in ("HEAD", "OPTIONS"):
            self.wfile.write(body)

    def request_path(self):
        parsed = urllib.parse.urlsplit(self.path)
        path = urllib.parse.unquote(parsed.path)
        path = posixpath.normpath(path)
        self.seen.append((self.command, path))
        params = urllib.parse.parse_qs(parsed.query)
        expected = "write" if path.startswith("/dst") else "read"
        if params.get("authz") != [expected]:
            self.reply(403)
            return None
        return path

    def do_OPTIONS(self):
        self.reply(200, headers={"Allow": "PROPFIND,GET,HEAD,PUT,OPTIONS",
                                 "DAV": "1"})

    def do_HEAD(self):
        path = self.request_path()
        if path is None:
            return
        if path in self.files:
            self.reply(200, headers={
                "Content-Length": str(len(self.files[path]))})
        elif path in self.uploads:
            self.reply(200, headers={
                "Content-Length": str(len(self.uploads[path]))})
        elif path in ("/src/tree", "/src/tree/nested", "/src/partial",
                      "/src/partial/nested", "/src/traversal",
                      "/src/traversal-direct") or path in self.directories:
            self.reply(200)
        else:
            self.reply(404)

    def do_GET(self):
        path = self.request_path()
        if path is not None:
            if path in self.files:
                self.reply(200, self.files[path])
            else:
                self.reply(404)

    def do_PUT(self):
        path = self.request_path()
        if path is not None:
            length = int(self.headers.get("Content-Length", "0"))
            self.uploads[path] = self.rfile.read(length)
            self.reply(201)

    def do_MKCOL(self):
        path = self.request_path()
        if path is not None:
            self.directories.add(path)
            self.reply(201)

    @staticmethod
    def item(path, directory=False, property_href=None):
        kind = "<D:collection/>" if directory else ""
        size = len(Handler.files.get(path, b""))
        extra = ("<D:href>" + xml.sax.saxutils.escape(property_href) +
                 "</D:href>") if property_href else ""
        return ("<D:response><D:href>" + xml.sax.saxutils.escape(path) +
                "</D:href><D:propstat><D:prop><D:resourcetype>" + kind +
                "</D:resourcetype><D:getcontentlength>" + str(size) +
                "</D:getcontentlength>" +
                extra + "</D:prop></D:propstat></D:response>")

    def do_PROPFIND(self):
        path = self.request_path()
        if path is None:
            return
        if path == "/src/partial/nested":
            self.reply(403)
            return
        children = {
            "/src/tree": [("good.bin", False, None), ("nested", True, None)],
            "/src/tree/nested": [("leaf.bin", False, None)],
            "/src/partial": [("good.bin", False, None),
                             ("nested", True, None)],
            "/src/traversal": [
                ("safe.bin", False, "../../escaped/payload.bin")],
            "/src/traversal-direct": [("..", False, None)],
        }
        if (path not in children and path not in self.files and
                path not in self.directories):
            self.reply(404)
            return
        entries = [self.item(path, path in children or
                             path in self.directories)]
        if self.headers.get("Depth") != "0":
            entries.extend(
                self.item(path + "/" + name, directory, override)
                for name, directory, override in children.get(path, []))
        body = ("<D:multistatus xmlns:D='DAV:'>" + "".join(entries) +
                "</D:multistatus>").encode()
        self.reply(207, body, {"Content-Type": "application/xml"})


def run(xrdcp, *args, success=True):
    result = subprocess.run([xrdcp, "--nopbar", *args],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=30)
    if (result.returncode == 0) != success:
        raise AssertionError((args, result.returncode, result.stdout,
                              result.stderr))


def main(xrdcp):
    server = Server(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = "http://127.0.0.1:" + str(server.server_port)
    try:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "upload.bin"
            source.write_bytes(b"upload data\n")

            # Both the probe and the upload require the write query.
            run(xrdcp, str(source), base + "/dst?authz=write")
            run(xrdcp, "-ODauthz=write", str(source), base + "/dst")
            assert Handler.uploads["/dst/upload.bin"] == source.read_bytes()
            run(xrdcp, str(source), base + "/dst", success=False)

            local_tree = root / "upload-tree"
            (local_tree / "nested").mkdir(parents=True)
            (local_tree / "nested/leaf.bin").write_bytes(b"uploaded leaf\n")
            run(xrdcp, "--recursive", "-ODauthz=write", str(local_tree),
                base + "/dst?test=path&extra=1")
            assert Handler.uploads["/dst/upload-tree/nested/leaf.bin"] == (
                local_tree / "nested/leaf.bin").read_bytes()

            for name, option, suffix in (
                    ("embedded", None, "?authz=read"),
                    ("option", "-OSauthz=read", ""),
                    ("combined", "-OSauthz=read", "?test=path&extra=1")):
                target = root / name
                target.mkdir()
                args = ["--recursive"]
                if option:
                    args.append(option)
                run(xrdcp, *args, base + "/src/tree" + suffix, str(target))
                assert (target / "tree/good.bin").read_bytes() == (
                    Handler.files["/src/tree/good.bin"])
                assert (target / "tree/nested/leaf.bin").read_bytes() == (
                    Handler.files["/src/tree/nested/leaf.bin"])

            # An incomplete subtree and an overridden href must both fail.
            for source_path in ("partial", "traversal"):
                target = root / source_path
                target.mkdir()
                run(xrdcp, "--recursive", "-OSauthz=read",
                    base + "/src/" + source_path, str(target), success=False)
                assert not (root / "escaped").exists()
                assert list(target.iterdir()) == []
            assert not any(method == "GET" and path == "/src/partial/good.bin"
                           for method, path in Handler.seen)
            assert not any(path == "/src/traversal/safe.bin"
                           for _, path in Handler.seen)

            target = root / "direct"
            target.mkdir()
            run(xrdcp, "--recursive", "-OSauthz=read",
                base + "/src/traversal-direct", str(target), success=False)
            assert list(target.iterdir()) == []
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main(sys.argv[1])

#!/usr/bin/env python3
"""Test-only GPI backend for durable prepare profile v1 (not installed).

XRD_PREP_MOCK_ROOT contains archive/, disk/, and backend/ directories. The
coordinator's request store is deliberately separate. Mutations are idempotent
by operation ID and query reports durable acknowledgements. No HTTP knowledge.
control.json accepts hold, failPaths, queryFailure and malformedQuery for tests.
"""

import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
from typing import List, Optional, Tuple
from urllib.parse import parse_qs

ID = re.compile(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\Z")
ARCHIVE_QUERY = "@xrdprep-v1:archiveinfo"


def sync_dir(path: Path) -> None:
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def atomic_json(path: Path, value: dict) -> None:
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=".tmp-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(value, stream, separators=(",", ":"), sort_keys=True)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
        sync_dir(path.parent)
    finally:
        with contextlib.suppress(FileNotFoundError):
            os.unlink(name)


def logical_path(value: str) -> str:
    if not value.startswith("/") or any(c in value for c in "\n\r\x00?#"):
        raise ValueError("invalid logical path")
    if any(part in (".", "..") for part in value.split("/")):
        raise ValueError("path traversal")
    return "/" + "/".join(part for part in value.split("/") if part)


def physical(root: Path, path: str) -> Path:
    result = (root / logical_path(path).lstrip("/")).resolve()
    if root.resolve() not in result.parents:
        raise ValueError("mock path escapes storage root")
    return result


def parse_files(arguments: List[str]) -> Tuple[List[dict], Optional[str]]:
    files, operations, seen = [], set(), set()
    for argument in arguments:
        path, _, opaque = argument.partition("?")
        path = logical_path(path)
        if path in seen:
            raise ValueError("duplicate path")
        seen.add(path)
        cgi = parse_qs(opaque, strict_parsing=True) if opaque else {}
        # Credentials must have been removed at the durable coordinator boundary.
        if "authz" in cgi:
            raise ValueError("credentials leaked to the mock backend")
        metadata = json.loads(bytes.fromhex(cgi.get("xrd.prepare.file", ["7b7d"])[0]))
        metadata["path"] = path
        files.append(metadata)
        if "xrd.prepare.operation" in cgi:
            operations.add(cgi["xrd.prepare.operation"][0])
    if len(operations) > 1:
        raise ValueError("inconsistent operation IDs")
    return files, next(iter(operations), None)


class Tape:
    def __init__(self, root: Path):
        self.root = root
        self.archive = root / "archive"
        self.disk = root / "disk"
        self.records = root / "backend" / "requests"
        for path in (self.archive, self.disk, self.records):
            path.mkdir(mode=0o700, parents=True, exist_ok=True)

    def request_path(self, request_id: str) -> Path:
        if not ID.fullmatch(request_id):
            raise ValueError("invalid request ID")
        return self.records / request_id[:2] / request_id[2:4] / (request_id + ".json")

    def read(self, request_id: str) -> Optional[dict]:
        try:
            return json.loads(self.request_path(request_id).read_text())
        except FileNotFoundError:
            return None

    def control(self) -> dict:
        try:
            return json.loads((self.root / "control.json").read_text())
        except FileNotFoundError:
            return {}

    def refresh(self, record: dict) -> None:
        control = self.control()
        if control.get("hold"):
            return
        for file in record["files"]:
            if file["state"] not in ("SUBMITTED", "STARTED"):
                continue
            source, target = physical(self.archive, file["path"]), physical(self.disk, file["path"])
            if file["path"] in control.get("failPaths", []) or not source.is_file():
                file.update(state="FAILED", error="mock archive file unavailable")
            elif not source.stat().st_size:
                file.update(state="FAILED", error="zero-length archive file")
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                if not target.exists():
                    temporary = target.with_name(target.name + ".staging")
                    shutil.copyfile(source, temporary)
                    with temporary.open("rb") as stream:
                        os.fsync(stream.fileno())
                    os.replace(temporary, target)
                    sync_dir(target.parent)
                file["state"] = "COMPLETED"
                file["pinned"] = True
            file["finishedAt"] = int(time.time())

    def locality(self, files: List[dict]) -> List[dict]:
        result = []
        for file in files:
            path = file["path"]
            disk = physical(self.disk, path).is_file()
            tape = physical(self.archive, path).is_file()
            item = {"path": path}
            if disk and tape:
                item["locality"] = "DISK_AND_TAPE"
            elif disk:
                item["locality"] = "DISK"
            elif tape:
                item["locality"] = "TAPE"
            else:
                item["error"] = "file does not exist"
            result.append(item)
        return result

    def query(self, request_id: str, files: List[dict]) -> dict:
        result = dict(schema=1, requestId=request_id, known=False, acknowledged=[], files=[])
        if request_id == ARCHIVE_QUERY:
            result.update(known=True, files=self.locality(files))
            return result
        record = self.read(request_id)
        if record is not None:
            self.refresh(record)
            atomic_json(self.request_path(request_id), record)
            public = []
            for file in record["files"]:
                public.append({k: file[k] for k in ("path", "state", "startedAt", "finishedAt", "error") if k in file})
            result.update(known=True, acknowledged=list(record["operations"]), files=public)
        return result

    def evict_unpinned(self, path: str) -> None:
        for record_path in self.records.glob("*/*/*.json"):
            record = json.loads(record_path.read_text())
            for file in record["files"]:
                if file["path"] == path and file.get("pinned"):
                    return
        target = physical(self.disk, path)
        with contextlib.suppress(FileNotFoundError):
            target.unlink()
            sync_dir(target.parent)

    def mutate(self, request_id: str, operation: str, files: List[dict], operation_id: Optional[str]) -> None:
        if not operation_id or not operation_id.startswith(request_id + ":"):
            raise ValueError("mutation requires a stable operation ID")
        fingerprint = hashlib.sha256(json.dumps([operation, files], sort_keys=True).encode()).hexdigest()
        record = self.read(request_id)
        if record and operation_id in record["operations"]:
            if record["operations"][operation_id] != fingerprint:
                raise ValueError("operation ID reused with different input")
            return
        if operation == "stage":
            if record is not None:
                raise ValueError("request already exists with another stage operation")
            now = int(time.time())
            record = dict(schema=1, requestId=request_id, operations={}, files=[
                dict(file, state="STARTED", startedAt=now, pinned=False) for file in files])
            self.refresh(record)
        else:
            if record is None:
                raise ValueError("request is unknown")
            selected = {file["path"] for file in files}
            if not selected.issubset({file["path"] for file in record["files"]}):
                raise ValueError("file does not belong to request")
            for file in record["files"]:
                if file["path"] not in selected:
                    continue
                if operation == "cancel" and file["state"] in ("SUBMITTED", "STARTED"):
                    file.update(state="CANCELLED", finishedAt=int(time.time()))
                elif operation == "evict":
                    file["pinned"] = False
        record["operations"][operation_id] = fingerprint
        atomic_json(self.request_path(request_id), record)
        if operation == "evict":
            for file in files:
                self.evict_unpinned(file["path"])
        # Fail AFTER the durable acceptance, reproducing a lost acknowledgement.
        if self.control().get("failAfterAccept"):
            raise RuntimeError("injected lost acknowledgement")


def main(arguments: List[str]) -> int:
    try:
        separator = arguments.index("--")
        request_id, operation, *paths = arguments[separator + 1:]
        if operation not in ("stage", "query", "cancel", "evict"):
            raise ValueError("unsupported mock operation")
        files, operation_id = parse_files(paths)
        root = Path(os.environ["XRD_PREP_MOCK_ROOT"])
        if not root.is_absolute():
            raise ValueError("mock root must be absolute")
        tape = Tape(root)
        # Independent processes spawned by GPI share one mock storage service.
        with (root / "backend" / ".lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            control = tape.control()
            with (root / "backend" / "calls.jsonl").open("a") as stream:
                stream.write(json.dumps(dict(requestId=request_id, operation=operation,
                                             operationId=operation_id, files=files)) + "\n")
            if operation == "query":
                if control.get("queryFailure"):
                    raise RuntimeError("injected query failure")
                if control.get("malformedQuery"):
                    print("not-json")
                else:
                    print(json.dumps(tape.query(request_id, files), separators=(",", ":")))
            else:
                tape.mutate(request_id, operation, files, operation_id)
        return 0
    except (ValueError, KeyError, OSError, RuntimeError) as error:
        print(f"mock prepare: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

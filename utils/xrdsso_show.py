#!/usr/bin/env python3
import base64
import datetime as dt
import json
import pathlib
import sys


def b64url_decode(seg: str) -> bytes:
    seg = seg.strip()
    seg += "=" * (-len(seg) % 4)
    return base64.urlsafe_b64decode(seg.encode("ascii"))


def read_token(path: pathlib.Path) -> str:
    token = path.read_text(encoding="utf-8").strip()
    if token.startswith("Bearer "):
        token = token[7:].strip()
    return token


def fmt_epoch(value):
    try:
        iv = int(value)
        return dt.datetime.utcfromtimestamp(iv).isoformat() + "Z"
    except Exception:
        return None


def main():
    if len(sys.argv) != 2:
        print("Usage: xrdsso_show.py <tokenfile>", file=sys.stderr)
        sys.exit(1)

    path = pathlib.Path(sys.argv[1])
    if not path.exists():
        print(f"error: token file not found: {path}", file=sys.stderr)
        sys.exit(1)

    token = read_token(path)
    parts = token.split(".")
    if len(parts) != 3:
        print("error: token is not a JWT (expected 3 segments)", file=sys.stderr)
        sys.exit(1)

    try:
        header = json.loads(b64url_decode(parts[0]).decode("utf-8"))
        payload = json.loads(b64url_decode(parts[1]).decode("utf-8"))
    except Exception as exc:
        print(f"error: failed to decode token: {exc}", file=sys.stderr)
        sys.exit(1)

    print("HEADER:")
    print(json.dumps(header, indent=2, sort_keys=True))
    print("\nPAYLOAD:")
    print(json.dumps(payload, indent=2, sort_keys=True))

    exp = payload.get("exp")
    iat = payload.get("iat")
    nbf = payload.get("nbf")
    if exp is not None or iat is not None or nbf is not None:
        print("\nTIME CLAIMS (UTC):")
        if iat is not None:
            print(f"  iat: {iat} ({fmt_epoch(iat)})")
        if nbf is not None:
            print(f"  nbf: {nbf} ({fmt_epoch(nbf)})")
        if exp is not None:
            print(f"  exp: {exp} ({fmt_epoch(exp)})")


if __name__ == "__main__":
    main()

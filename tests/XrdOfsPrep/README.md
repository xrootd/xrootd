# XrdOfsPrep prepare test suite

Tests the durable prepare persistence wrapper (`libXrdOfsPrepPersist.so`) and the
filesystem tape mock it drives, from store-level unit tests up to a loopback
HTTP/native integration fixture.

## Layout

| File | Purpose |
| --- | --- |
| `XrdOfsPrepTests.cc` | GoogleTest for the sharded store and the persistence coordinator. |
| `mock_tape.py` | CI-only GPI program that simulates an archive/disk tape backend. |
| `test_mock_tape.py` | Python unit tests for the mock. |
| `integration.py` | Loopback `HTTP -> bridge -> OFS -> wrapper -> GPI` fixture. |
| `TestAuth.cc` | Synthetic `XrdAccAuthorize` library: `alice` and `bob` are allowed, everyone else is denied. |
| `CMakeLists.txt` | Registers the CTest targets described below. |

## The tape mock

`mock_tape.py` is a non-installed GPI program spawned by `libXrdOfsPrepGPI.so` as

```
mock_tape.py [gpi options] -- <request-id> <stage|query|cancel|evict> <logical-path?opaque> ...
```

It models tape as two directories under the root named by `XRD_PREP_MOCK_ROOT`:

- `archive/` — the tape; the authoritative copy of each file.
- `disk/` — the disk cache where staged files land.
- `backend/requests/` — the mock's own sharded, durable per-request records.
- `backend/.lock` and `backend/calls.jsonl` — the process lock and call log.

`stage` copies `archive` to `disk`, `evict` removes a disk copy only when no
request still pins it, and `query` reports per-file state or locality. Operations
are idempotent by operation ID, so a replayed dispatch is acknowledged exactly
once. Queries speak the same replay-safe profile v1 as the coordinator (see
`src/XrdHttpTapeApi/README.md`).

`control.json` in the mock root injects failures:

| Key | Effect |
| --- | --- |
| `hold` | Leave files in `SUBMITTED`/`STARTED` instead of completing. |
| `failPaths` | Mark the listed paths `FAILED`. |
| `queryFailure` | Make every query fail, as if the backend were unavailable. |
| `malformedQuery` | Emit non-JSON query output. |
| `failAfterAccept` | Raise after durable acceptance, simulating a lost acknowledgement. |

## Tests

CTest registers targets under the `XrdOfsPrep::` prefix:

- **`XrdOfsPrep::PrepStore.*` / `PrepProtocol.*` / `PrepPersist.*`** (gtest) —
  store sharding, atomic revision, single-writer locking, and corruption/version
  rejection; profile path/metadata helpers; admission, ownership, subset cancel,
  tombstones, and recovery after a lost reply or backend outage.
- **`XrdOfsPrep::Mock`** — the Python mock unit tests, including replay after a
  lost acknowledgement and credential-boundary checks.
- **`XrdOfsPrep::Integration`** — a loopback fixture driving real HTTP and
  `xrdfs` requests through the bridge, OFS, the wrapper, and the GPI mock,
  including native wire session authentication vs per-path CGI authorization,
  subset cancellation, release, restart recovery, and failure injection.

The synthetic authorization library is test-only and never installed. The
SciTokens-authenticated `XrdClHttp::tape` fixture (`tests/XrdClHttp/`) reuses
the same mock through the full server stack.

## Running

```
cmake -S . -B build -DENABLE_TESTS=ON -DENABLE_SERVER_TESTS=ON
cmake --build build -j4
ctest --test-dir build -R '^XrdOfsPrep::' --output-on-failure
```

The Python tests can also be run directly:

```
python3 test_mock_tape.py
XRD_PREP_MOCK_ROOT=/tmp/tape python3 mock_tape.py -- <request-id> stage /path
```

# XrdOfsPrep prepare test suite

Tests the durable prepare persistence wrapper (`libXrdOfsPrepPersist.so`) and the
filesystem tape mock it drives. This layer runs without the HTTP adapter.

## Layout

| File | Purpose |
| --- | --- |
| `StorageContract.hh` | Reusable store contract assertions using only the public API. |
| `MemoryStorage.hh` | Independent typed test store with fault injection. |
| `XrdOfsPrepTests.cc` | GoogleTest for the sharded store and the persistence coordinator. |
| `mock_tape.py` | CI-only GPI program that simulates an archive/disk tape backend. |
| `test_mock_tape.py` | Python unit tests for the mock. |
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
`src/XrdOfs/README.prepare-storage.md`).

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
- **`XrdOfsPrep::NativePrepare.*`** — typed in-process callbacks, durable per-file
  dispatch, acknowledgement recovery, safe retry versus uncertain outcomes,
  cancellation during callbacks, retention in one record and locality queries.
- **`XrdOfsPrep::Mock`** — the Python mock unit tests, including replay after a
  lost acknowledgement and credential-boundary checks.
This HTTP layer adds `integration.py`, `TestAuth.cc` and `TestHttpHandler.cc`:
loopback HTTP/native, synthetic authorization and real legacy/bridge lifecycle
fixtures. The shared backend documentation remains in the coordinator layer.
`XrdOfsPrep::Integration` exercises these fixtures; `XrdClHttp::tape` is the
separate SciTokens-authenticated client integration.

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

## Installed consumer and package manifests

After a complete build, install into a disposable prefix and compile without
source/private include directories:

```sh
cmake --install build --prefix /tmp/xrootd-sdk
cmake -S tests/XrdOfsPrep/installed-consumer -B /tmp/prep-consumer -DXROOTD_PREFIX=/tmp/xrootd-sdk
cmake --build /tmp/prep-consumer
ctest --test-dir /tmp/prep-consumer --output-on-failure
python3 tests/XrdOfsPrep/check-packaging.py . /tmp/xrootd-sdk
```

The manifest check covers both Debian library naming variants, runtime SONAME,
plugin and development ownership in RPM and Debian. It does not build binary
packages or replace a distribution's package test pipeline.

## Sanitizer configurations

Use separate build directories. For ASan/UBSan configure with
`-DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`
and the same `CMAKE_C_FLAGS`, then build/run the GPI and coordinator targets.
For TSan use `-fsanitize=thread -fno-omit-frame-pointer` instead. Run
`NativePrepare.ForegroundObservationsCanOverlapWorkerAndEachOther` and the full
native suite. The overlap test gates the worker and two independent foreground
observations; it does not use a sleep to create the interleaving. Sanitizers need
host support; an initialization failure is not a passing test.

# Durable prepare storage API

`XrdOfsPrepPersist` owns request admission, authorization/ownership, operation
ordering, recovery and retention. `XrdOfsPrepStorage` owns atomic durable request
records. Native sites implement `XrdOfsPrepBackend` for tape operations and namespace
observations. The coordinator journals each per-file dispatch and outcome in the
same request record; sites need no second registry, wire codec, or journal cleaner.
External programs can still use `XrdOfsPrepare` and its profile v1 protocol.

The installed `XrdOfsPrepPersist.hh`, `XrdOfsPrepBackend.hh` and
`XrdOfsPrepStorage.hh` headers and
`libXrdOfsPrepPersist` shared library allow an SFS implementation such as EOS to
reuse the coordinator without using the OFS loader or the default file store.
This is a C++17 interface; build consumers with a compatible C++ ABI. Its initial
library SONAME is 1. The existing versioned prepare plugin entry point is unchanged.

## Implementing a store

The boundary contains typed records, owners, files, operations and metadata values.
It has no JSON-library, filesystem, directory, or HTTP types. Metadata values
preserve nested objects/arrays, null, booleans, strings and numeric values. A store
can use native database fields, Protobuf, or another representation. The existing
JSON representation remains private to the file store and coordinator's internal
wire model; this change does not replace the JSON backend/HTTP wire profile.

| Operation | Required behavior |
| --- | --- |
| `Create(record)` | Atomically create the entire manifest and initial intent, without replacement; return revision 1, or `EEXIST`. |
| `Read(id)` | Return a complete committed record; `ENOENT` means absent, never unavailable or corrupt. |
| `Update(record, expected)` | Atomically compare revision and replace the whole record; return `expected + 1`, `EAGAIN` on conflict or `ENOENT` if absent. |
| `Erase(id, expected)` | Remove only that revision, with the same conflict/missing semantics. |
| `List()` / `Cursor::Next(limit)` | Incremental backend-owned scan; bounded pages, including empty intermediate pages, and an explicit end marker. |

A scan need not be a snapshot. It must visit records present throughout the scan;
duplicates are allowed. New admissions are blocked until recovery completes, and
concurrent admissions after recovery schedule themselves. The coordinator resets
and retries failed cursors. A corrupt individual record is logged and does not
prevent other records from being recovered. Keep cursors within the store lifetime.

A successful mutation means **durable**, not merely visible to a subsequent read.
A failed write can have committed: `StorageError::Outcome::Unknown` requests
reconciliation; `Unchanged` guarantees it did not change storage. Unclassified
exceptions are conservatively uncertain. The coordinator preserves ambiguous
intent. Native calls are made only after a durable dispatch write; uncertain
writes or replies enter recovery, never an unconditional second dispatch.
Bound storage I/O and backend execution times; destruction joins the worker.

Implementations must serialize concurrent calls and enforce one active coordinator
per namespace, either with an exclusive store lock or site leadership that covers
all in-flight backend calls. Revision checks prevent lost record updates; they do
not fence external tape operations. Shared QDB storage alone does not enable
active-active service. Any future lease design must cover both record updates and
backend effects. Automatic recovery of uncertain effects needs authoritative
backend outcome lookup or deduplication by operation ID and file path.

## Implementing a native backend

Implement two callbacks using only public C++ types:

- `Execute(requestId, operationId, kind, file)` performs one stage, cancel or
  eviction. The file carries typed metadata and advisory disk lifetime. The pair
  `(operationId, file.path)` identifies the side effect; `requestId` groups a recall.
- `Observe(path)` reports disk/tape locality and namespace lookup errors shared
  by all requests for that path. It cannot report request-specific recall errors. A file becomes
  completed only after its stage dispatch was acknowledged and it is online.

`Execute(Stage)` returns `Applied` after durable acceptance (not necessarily
recall completion), `Rejected` for a definite failure, `Retry` when it is certain no
operation was dispatched, or `Unknown` when an effect may have happened. Exceptions
are also unknown. Required site metadata writes must be durable before `Applied`.
For cancellation, `Applied` has the stronger meaning that this request's interest
has been durably cancelled. It need not abort a shared physical tape recall.
Queuing a cancellation command is `Unknown` until `Recover` establishes its outcome.
An already terminal recall and its timestamps are preserved. An external profile
backend instead reports asynchronous cancellation outcomes in its request query.

An optional `Recover` callback receives the same IDs, kind and file after a crash
or unknown outcome. It must use authoritative lookup or backend deduplication;
return `Retry` only after proving the original call cannot still take effect.
The default returns `Unknown`, retaining the request for reconciliation. Namespace
locality alone cannot establish cancellation/release acknowledgement. Other files
in the same operation can progress; later operations wait for the unresolved one.

Per-file dispatch and acknowledgement live in `Record::operations[].files`.
These file states describe dispatch outcomes; `Record::files` describes the recall
lifecycle. The entire record is updated atomically and retired together, eliminating
an independently retained backend journal. No store schema extension is required.
Use a distinct backend identity when migrating from an external profile/journal:
old backend acknowledgements are not interchangeable with native dispatch results.

Callbacks run without registry locks, and may overlap request admission/status
calls. Execute/Recover run serially on the worker; foreground archiveinfo can
call Observe concurrently with them and with other Observe calls. Implementations
must synchronize shared state. The site must fence active
leadership and use bounded IO; the coordinator cannot cancel a blocking callback.

## Site integration

```cpp
#include <XrdOfs/XrdOfsPrepPersist.hh>

// MyStore implements the installed storage interface. Its writer guard is
// acquired before construction returns and outlives the coordinator worker.
auto store = std::make_unique<MyStore>(siteDatabase);
auto coordinator = std::make_unique<XrdOfsPrepPersist>(
    std::move(store), "site-backend-v1", sitePrepareBackend,
    authorizationEnvironment, logger);
```

Route native SFS prepare/query/cancel/evict calls through the coordinator, following
the existing OFS dispatch rules. Publish `coordinator->PublishProfile(runtimeEnv)`
only once that route is installed. The Tape REST handler tests profile v1, not the
presence of the file plugin. Keep the site backend, authorization environment and
logger alive until coordinator destruction finishes. Acquire leadership before
construction; drain/destroy the coordinator before relinquishing leadership.

The authorization environment must contain `XrdAccAuthorize*` or the explicit
`XrdOfsPrepAuthorizer*` SFS callback from the installed coordinator header. Null
or missing authorization fails closed. The SFS callback verifies this request's
credentials, checks the supplied path/CGI and returns a typed owner. The coordinator
calls it for every selected file, including stored paths on ID-only requests,
then checks a consistent owner and subset membership. Issuer plus subject identify
a token owner even when different users map to one service account. Refreshed
credentials for the same principal remain valid. Connection attributes are never
a substitute for fresh verification. Do not route service replay into admission.

## Default implementation and compatibility

The existing root-based constructor and `ofs.preplib ++` configuration still use
the JSON file store. `XrdOfsPrepFileStorage(root)` also exposes it through the new
interface. UUID sharding, schema 1, file permissions, exclusive root lock and
file/directory synchronization are unchanged; existing request records are read
without migration. File revision checks are serialized under the root writer lock.

`tests/XrdOfsPrep/StorageContract.hh` is reusable by out-of-tree backend tests. It
checks create collisions, complete typed records, competing revisions, conditional
erase and bounded scans. The in-tree tests run it against the file implementation
and an independent typed memory implementation, then exercise coordinator recovery,
retention, failed scans and uncertain writes without filesystem access. A database
implementation additionally needs real durability/restart and leadership tests.

## Record validation and dispatch states

Every store read used by the coordinator passes the same semantic validation:
UUID and owner shape, bounded normalized unique manifest/metadata, sequential
operation IDs, full initial stage, immutable manifest membership, bounded operation
count, valid enums and timestamp/state presence. Corruption returns EIO and is
logged; it never authorizes new backend work or replacement records. Backend and
coordinator timestamps may come from different clocks and are not ordered against
each other.

Schema 1 retains `File` in operation entries for storage compatibility. In native
operation entries SUBMITTED means not dispatched, STARTED means uncertain/in flight,
COMPLETED means acknowledged, FAILED means rejected, and CANCELLED means suppressed
before dispatch. These are distinct from the recall states in the manifest. A new
C++ type would require an out-of-tree codec migration without changing wire behavior;
that change is deliberately deferred while these semantics are documented and tested.

## External plugin configuration

```conf
ofs.preplib libXrdOfsPrepGPI.so -admit stage,query,cancel,evict -cgi -maxfiles 48 -maxresp 1m -run /opt/site/bin/prepare
ofs.preplib ++ libXrdOfsPrepPersist.so /var/lib/xrootd/prepare site-backend-v1 replay-safe
```

Wrapper arguments are:

```
<absolute-state-root> <backend-identity> replay-safe [maxrequests=N] [retention=N]
```

`maxrequests` defaults to 10000 active/reconciling requests. It caps new
stage admission: when the active queue reaches `maxrequests`, new stage submissions
are rejected (yielding 429 / `EDQUOT`; 503 / `EAGAIN` is reserved for startup recovery
before local state has reconciled). Existing request reactivations (such as
subset cancellations or releases on already admitted requests) and startup recovery
bypass this admission threshold so that operator/client actions on accepted intent
and service recovery are never locked out. `retention` is a
positive number of seconds, default 604800 (seven days), measured from stage
completion. Records with unacknowledged operations are never expired. Retention
is independent of requested disk residency. `backend-identity` must remain
stable across restarts and must change when switching unrelated backends; a
mismatch prevents replay of that record.

`replay-safe` is an explicit operator assertion that the backend implements
idempotent operation IDs and authoritative, durable acknowledgements. It is not
an automatically detected capability. Backend calls must have bounded execution
time: the coordinator delegates to the backend plugin or GPI script synchronously
on its worker thread and does not impose a hard kill timeout. A script
should submit/query work and return promptly, not remain running for the duration
of a tape recall. Sites must enforce bounded execution (for instance via an
external wrapper such as `timeout(1)` or internal deadlines in compiled integrations) to avoid
worker starvation.


## External backend profile v1

The wrapper always supplies a UUID request ID to the backend, including eviction.
A script receives the existing GPI argument form:

```
[prepare options] -- <request-id> <stage|query|cancel|evict> <logical-path?opaque> ...
```

GPI must be configured with `-cgi`. Each file has independent opaque data:

* `xrd.prepare.file`: lowercase hex encoding of a JSON object containing supported
  `diskLifetime` (the original duration string) and `targetedMetadata` (object).
* `xrd.prepare.operation`: stable `<request-uuid>:<sequence>` on every mutation.
  It identifies an immutable operation and must be deduplicated by the backend.

Credentials and HTTP headers are not passed to the backend. The operation is
already authorized and recovery runs as the service, not with an expired user
token. Site backends must provision their own downstream service authorization.

A query returns `SFS_DATA` (script: JSON on stdout, exit zero) with this envelope:

```json
{"schema":1,"requestId":"UUID","known":true,
 "acknowledged":["UUID:1"],
 "files":[{"path":"/store/file","state":"COMPLETED",
           "startedAt":100,"finishedAt":101}]}
```

A known response must contain exactly the requested manifest's file set, with no
duplicates. Valid file states are `SUBMITTED`, `STARTED`, `COMPLETED`, `FAILED`,
`CANCELLED`; timestamps are unsigned epoch seconds and `error` is an optional
bounded string. An authoritative unknown response is `known:false` with empty
`files` and `acknowledged` arrays. Failure to contact the tape system must instead
fail the query. Malformed, oversized or truncated output is never success.

Acknowledgements must survive backend restart. Repeating the same operation ID
and arguments must not repeat its logical effects; reuse with different arguments
must fail. Keep request/operation associations for at least as long as the
coordinator may retain and replay the record, including prolonged outages.
There is no generic exactly-once guarantee for a nonconforming external system,
nor a backend-forget protocol in this first version.


Profile v1 admits at most 48 files per request. Raising only GPI `-maxfiles`
does not raise the coordinator limit. Clients must submit batches of at most 48;
requests are never silently split. `-wait` limits query-slot admission, not program
execution. The GPI plugin is startup-only: restart to change its configuration.

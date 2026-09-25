# XrdHttpTapeApi: native prepare adapter

The Tape REST handler translates WLCG requests into the authenticated
XrdHttp/XRootD bridge. It contains no tape simulator and never copies or removes
storage replicas. The request path is:

```
HTTP -> XrdHttp bridge -> XrdXrootdProtocol -> XrdOfs::prepare
                                            |
native prepare/query -----------------------+
                                            |
                               XrdOfsPrepPersist (optional wrapper)
                                            |
                          XrdOfsPrepare plugin or XrdOfsPrepGPI script
```

This is the first implementation of the durable backend **profile v1**, towards
issue #2922. It is opt-in; legacy prepare plugins retain their existing interface.
A plugin must implement the profile below before it can be wrapped safely. Merely
implementing the `begin`, `cancel` and `query` methods is not sufficient.

## Configuration

For a site script (which must be an executable and return promptly):

```conf
all.sitename example
ofs.authorize 1
# Configure the site's authentication and authorization libraries as usual.
http.header2cgi Authorization authz strip-on-redirect

ofs.preplib libXrdOfsPrepGPI.so -admit stage,query,cancel,evict -cgi -maxfiles 48 -maxresp 1m -run /opt/site/bin/prepare
ofs.preplib ++ libXrdOfsPrepPersist.so /var/lib/xrootd/instance/prepare site-backend-v1 replay-safe
http.exthandler xrdhttptapeapi libXrdHttpTapeApi.so 4m
```

A compiled site backend can replace the first `ofs.preplib` line. The persistent
wrapper uses the existing `XrdOfsAddPrepare` entry point; no new prepare plugin ABI
is introduced. The handler requires this profile to be configured at startup.
The previous simulator handler parameter (a storage root) is no longer accepted.
The optional handler argument is now only the maximum JSON body size (default
4 MiB). No migration of the former simulator's request files is provided.

HTTP bodies must use `Content-Length`; chunked request bodies receive HTTP 411.
`Expect: 100-continue` is supported after checking the configured size limit.
Bodies are read incrementally across TLS records and protocol buffers. Oversized
or incomplete bodies close the connection so unread bytes cannot be interpreted
as another request.

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

## Native SFS integrations

An SFS such as EOS can link `libXrdOfsPrepPersist`, supply an
`XrdOfsPrepStorage` implementation and implement `XrdOfsPrepBackend` directly.
The typed callbacks execute stage/cancel/release and observe file locality. The
coordinator records per-file dispatch and acknowledgement in its request record;
no backend JSON/CGI codec, second journal or separate journal cleaner is needed.
See [the native backend and storage API](../XrdOfs/README.prepare-storage.md).

Native callbacks explicitly distinguish accepted operations, definite failures,
safe retries and uncertain effects. After an uncertain call or restart, the
coordinator invokes `Recover` instead of blindly dispatching again. Its default
leaves the operation pending until reconciled. Automatic progress across that
failure window requires authoritative backend lookup or deduplication.

The SFS routes prepare through the coordinator and publishes its profile only
once the route is installed. HTTP configuration is the same, with no `ofs.preplib`
wrapper required in that SFS. The external-program protocol described below
remains supported for GPI and existing prepare plugins.

## Persistent store and recovery

The coordinator uses a storage interface, with the JSON file implementation as
the default. EOS and other SFS implementations can inject another store and link
the coordinator library; see [the storage API](../XrdOfs/README.prepare-storage.md).
Only the coordinator writes its request namespace. An external backend that
maintains its own acknowledgement database must use a different namespace. The default file store keeps records as:

```
<state-root>/.lock
<state-root>/requests/<UUID first 2 hex>/<UUID next 2 hex>/<UUID>.json
```

UUIDv4 prefixes provide uniform fan-out without a directory scan on lookup. There
is one bounded record per bulk request, not one file per tape file. Schema 1
records contain ownership, the immutable file manifest and metadata, observed
states, ordered operation intents, backend identity, revision and a tombstone.
Each successful update writes and synchronizes a unique temporary file,
atomically publishes/replaces the record and synchronizes its directory. New
shard/ancestor directory entries are synchronized too. The root is service-owned
and private; records are mode 0600. A nonblocking process lock prohibits a second
writer. A corrupt record is an error, not a missing request or permission to
submit a replacement.

Successful stage submission means **durable admission to the configured store**, not backend
acceptance or completed staging. The worker records dispatch intent before
invoking the backend. For the external profile, `SFS_OK` only means dispatch; query must explicitly
acknowledge the stable operation ID before the intent is marked done. After a
crash or ambiguous result the same operation ID is replayed, never an unrelated
new backend request. A query failure is not interpreted as an unknown request.

Recovery scans incrementally, reconstructs an in-memory active index, and blocks
new stage admission until the initial scan completes. Status is cached in the
registry: HTTP polling does not spawn a script per GET. A single worker performs
ordered reconciliation; normal refresh is once per second and failures back off
up to 32 seconds. Calls occur outside record locks. Concurrent mutation preserves
new intent, and late observations cannot regress terminal file outcomes.

## External backend profile v1

The complete backend query/acknowledgement protocol and plugin configuration
are documented in [the coordinator layer](../XrdOfs/README.prepare-storage.md#external-backend-profile-v1).
That layer can be built and tested independently of HTTP.

## REST operations and authorization

| REST operation | Native translation |
| --- | --- |
| POST `/api/v1/stage` | `kXR_prepare` + `kXR_stage` |
| GET `/api/v1/stage/<id>` | `kXR_query` + `kXR_QPrep` |
| POST `/api/v1/stage/<id>/cancel` | `kXR_prepare` + `kXR_cancel` |
| POST `/api/v1/release/<id>` | `kXR_prepare` + `kXR_evict`, with per-file `xrd.prepare.request=<id>` |
| DELETE `/api/v1/stage/<id>` | cancel with reserved request ID `@xrdprep-v1:delete:<id>` |
| POST `/api/v1/archiveinfo` | QPrep with reserved request ID `@xrdprep-v1:archiveinfo` and file list |

The two reserved IDs are provisional profile conventions, not new XRootD wire
opcodes. For the archive query the backend returns the same envelope with a file
list containing authoritative `locality` (`DISK`, `TAPE`, `DISK_AND_TAPE`, `NONE`)
or a per-file `error`, rather than stage state. The response must be known.

The whole cancel/release subset is validated before mutation. Cancellation is
request-scoped and best effort, not a promise to interrupt a physical tape read.
Release withdraws this request's disk-residency interest; the backend protects
other requests' interests and decides whether to evict. A completed stage remains
historically completed after release. DELETE hides the resource immediately with
a durable tombstone and cancellation intent; it does not delete archived data or
implicitly evict completed replicas. The tombstone survives until outstanding
operations have been acknowledged and retention permits cleanup.

OFS authorizes each supplied path using its opaque data. ID-only operations also
reauthorize the stored manifest and compare its owner; the request UUID is not a
capability. Token ownership uses the **verified issuer and subject**, not raw
JWT claims or connection IDs. For other mechanisms the initial policy is the
mapped name plus security protocol. Delegated/admin ownership overrides and
cross-mechanism certificate principal mapping are not implemented. This version
retains native prepare's read-access authorization requirement; dedicated
stage-only/poll-only scope policy needs separate integration before such tokens
are advertised as supported.

In the native XRootD wire protocol (`XrdXrootdProtocol`), ID-only queries
(`xrdfs query prepare <id>`) carry no file paths and therefore transport no
per-path opaque info (CGI) over the wire; authorization relies on the connection's
authenticated session identity (`XrdSecEntity`). Conversely, native operations that
supply paths (stage, query with paths, subset cancel via `prepare -a <id> <paths>`,
and release via `prepare -e <paths>?xrd.prepare.request=<id>`) transport per-file
CGI with each path, allowing individual token authorization per path. On the HTTP
REST side, the bridge passes the Bearer token as CGI for all paths in stage,
cancel and release requests, and supplies transient request CGI for ID-only status
polling.

HTTP request CGI is transient bridge state, including on ID-only operations, and
is cleared at completion. It is never serialized into the registry. Existing
external handlers retain their lifecycle; only handlers deriving from the new
opt-in bridge interface request deferred native completion. Discovery remains
public and direct. A client disconnect after admission does not cancel the job.

## Boundaries of the initial implementation

The supported deployment is **one logical owning endpoint and one writer**. The
default store requires a persistent local POSIX filesystem with working
file/directory synchronization; custom stores must satisfy the same atomicity,
durability and exclusive-writer contract.
There is no active-active/shared-filesystem coordination or automatic request
routing. Native redirects are currently returned as a REST 503 rather than being
followed implicitly. Configure the advertised URL to the stable owning endpoint;
credentials are never forwarded to native redirect targets. Legacy requestless eviction is not accepted by this wrapper;
it must carry the request context documented above.

Batches are capped at 48 distinct files, paths at 1024 bytes, supported metadata
at 1024 bytes per file, and records/native responses at 4 MiB. Unsupported prepare
flags and unrepresentable paths are rejected before admission; batches are not
silently split. Configure FTS `StagingBulkSize` to at most 48 for this endpoint.
[WLCG operational guidance](https://twiki.cern.ch/twiki/bin/view/LCG/TapeRestAPI)
documents 200 in its FTS/dCache example, so existing client settings need checking.
Changing GPI `-maxfiles` alone does not change the adapter/coordinator limit.
A configurable larger limit requires a coherent profile/response-size design and
is deferred; this implementation keeps its tested bounded admission.
The initial coordinator has one worker, logs rather than a new
metrics API, and no spool-byte reservation or high-availability mechanism. It is
intentionally a reviewable first implementation, not a claim that every site
plugin now satisfies durable Tape REST semantics.

## Tests

`tests/XrdOfsPrep/mock_tape.py` is a CI-only, non-installed GPI program with its
own sharded backend records, durable acknowledgements and process locking. Set
`XRD_PREP_MOCK_ROOT` to a test directory containing `archive/` and `disk/`; keep
the coordinator registry separate. `control.json` can hold recalls, fail selected
paths, fail queries, emit malformed query output, or fail after durable acceptance.
The mock copies data only to simulate tape behavior and does not implement tape
mount scheduling or real disk-lifetime expiration.

CTest runs store/coordinator unit tests, seven Python mock tests, and a loopback
integration fixture that exercises actual HTTP/native requests through OFS, the
wrapper and GPI. The synthetic authorization library is test-only and never
installed. The existing SciTokens-authenticated `XrdClHttp::tape` fixture uses the
same GPI mock, with polling adapted for asynchronous completion.

```
cmake -S . -B build -DENABLE_TESTS=ON -DENABLE_SERVER_TESTS=ON
cmake --build build -j4
ctest --test-dir build -R '^XrdOfsPrep::' --output-on-failure
ctest --test-dir build -R '^XrdClHttp::tape$' --output-on-failure
```

Run server tests as a non-root user. The second command group requires the
existing SciTokens fixture dependencies. The dedicated Tape prepare workflow
builds the required modules and runs the first group without external services.

## Response and method contracts

Creation returns `requestId`; status returns the coordinator's `id`, epoch-second
`createdAt`/`startedAt` and (when complete) `completedAt`, plus per-file states and
terminal timestamps. Archiveinfo returns an array of path/locality objects.
These operation-specific names are checked by a real end-to-end fixture, not
inferred from a translation mock. See the
[WLCG reference](https://docs.google.com/document/d/1Zx_H5dRkQRfju3xIYZ2WgjKoOvmLtsafP2pKGpHqcfY/edit).

405 responses advertise the actual resource methods in `Allow`: GET for discovery,
POST for stage creation, cancel, release and archiveinfo, and GET/DELETE for a
request resource. HEAD errors carry headers without response content, following
[RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html#section-10.2.1).
Raw NUL bytes are rejected before JSON parsing; escaped NUL in arbitrary metadata
is still application data and path fields retain their normal validation.

The bridge is opt-in but common response helpers affect legacy external handlers
as well. A real non-opt-in fixture checks sequential keep-alive responses,
explicit connection close, partial-response failure and operation without bridge
login. Real bridge tests cover response overflow (which closes the connection),
credential changes and disconnect after durable admission. Test transport stubs
remain useful for adapter translation/lifetime assertions, not protocol proof.

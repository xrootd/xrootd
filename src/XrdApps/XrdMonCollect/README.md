# xrdmoncollect

`xrdmoncollect` reads XRootD detailed-monitoring UDP packets (the
`xrootd.monitor` streams), correlates the **`f` (file-stats) stream** against
the user dictionary, and writes **one JSON document per completed transfer**
(file close). The output is line-delimited JSON (NDJSON), the OpenSearch
`_bulk` format, or OTLP/JSON, suitable for ingestion into OpenSearch /
Elasticsearch, an OpenTelemetry collector, Loki, and so on.

This is the document-oriented half of the XRootD monitoring story. A companion
aggregate sink exposes bounded-cardinality Prometheus metrics over the same
decoded stream (see [Aggregated metrics](#aggregated-metrics-prometheus)); the
native server-side metrics live in the `XrdMetrics` component.

## Architecture

`xrdmoncollect` is a bounded, three-stage pipeline. The design goal is that a
slow or unreachable downstream sink never costs UDP monitoring packets: the
socket-draining stage is decoupled from decoding, and decoding is decoupled from
the (blocking) network POSTs by bounded, recycling hand-off queues.

```
 UDP :port
    │
    ▼
┌────────────────┐  recvPipe   ┌─────────────────────────┐
│ Receiver       │ ──────────▶ │ Serializer              │
│ (main thread)  │  (batches)  │ decode + correlate      │
└────────────────┘             │ (owns XrdMonDecode)     │
                               └───────────┬─────────────┘
                                 docSink fan-out
              ┌──────────────┬──────────────┴───────┬──────────────────┐
              ▼              ▼                       ▼                  ▼
        file / stdout   TCP forward           _bulk batch         OTLP batch
                                                   │ postPipe        │ otlpPipe
                                                   ▼                 ▼
                                          ┌──────────────┐   ┌──────────────┐
                                          │ OS output    │   │ OTLP output  │
                                          │ thread (POST)│   │ thread (POST)│
                                          └──────┬───────┘   └──────┬───────┘
                                            on failure           on failure
                                                 ▼                    ▼
                                            --cache-dir  ◀── replay oldest-first
```

### Pipeline stages

1. **Receiver** — the main thread. It does nothing but drain the UDP socket into
   pooled packet batches and hand them to the serializer through a bounded
   recycling queue. The kernel receive buffer is enlarged (`--rcvbuf`, default
   16M) and the queue is generously sized (`--queue-depth`, default 64). To
   prioritise *not losing packets* the receiver applies **backpressure** — it
   waits for a free batch rather than dropping — and combined with the large
   socket buffer it effectively never has to wait.
2. **Serializer** — a single thread that owns the `XrdMonDecode` instance
   exclusively. It decodes and correlates every packet, writes the file and TCP
   forward sinks inline, and accumulates one OpenSearch `_bulk` body and/or one
   OTLP batch per flush window (`--flush-count` packets or `--flush-secs`
   seconds, whichever comes first). Because it is the sole decoder, correlation
   state needs no locking.
3. **Output** — one dedicated thread per HTTP sink. Each performs the blocking
   POST (with retry) so that neither decoding nor reception ever waits on the
   network.

Work is handed between stages by `XrdMonPipe<T>` (`XrdMonPipe.hh`), a
single-producer/single-consumer bounded queue with buffer recycling
(`acquire`/`submit` on the producer side, `take`/`takeFor` on the consumer side,
`recycle` to return an emptied buffer). The receive queue holds `--queue-depth`
packet batches; the two POST queues hold `kPostQueueDepth` (16) bodies each. The
file and TCP-forward sinks are written synchronously in the serializer, so a
document they cannot absorb is **dropped and counted** (e.g. `fwd` drops while
the forward consumer is down); the HTTP sinks instead **cache-or-drop** (below).

### Correlation state

Decode state is kept **per server incarnation**, keyed by the sender address
plus the server start-of-day time (`src|stod`), so concurrent XRootD versions on
one host and restarts are separate incarnations. Each incarnation holds the
`u`/`d`/`i` (user, path, appinfo) dictionaries, the `T`/`U` (token, SciTags)
maps, and the **open-file table**. A file close is correlated by looking up its
`fileID` in that table to recover the LFN, open size, user, and open timestamp,
then joining the user dictionary to produce **one transfer document** per close
(see `XrdMonDecode.{hh,cc}`, the `Server` struct and `ServerFor`).

State is bounded so it cannot grow without limit when a close or disconnect is
lost (dropped datagram, crash, restart — the server never reuses a dictid within
an incarnation):

- `--max-memory` (default 256M; `0` = unbounded) caps state to an approximate
  byte budget, **LRU-evicting** cold entries first. Recency protects a genuine
  long-running transfer: each in-flight `xfr` snapshot and each reference of a
  session by a close promotes the entry, so a file left open for a day survives
  while memory allows.
- `--max-entries` adds an optional hard entry-count backstop (off by default).
- `--server-ttl` (default 86400s; `0` = never) reclaims whole incarnations idle
  past the TTL, so dead incarnations from restarts and rolling upgrades do not
  accumulate.

The consequences of eviction/loss for the *output* (orphan closes, documents
missing a field) are covered under [Limitations](#limitations).

### Serialization

Every document — the file-close transfer/access records, `session`,
`server_ident`, `frm`, `redirect`, the `t`-stream traces and `gstream` — shares
one OpenTelemetry-aligned schema: a process-level `resource` object and an
event-level `attributes` object, both keyed by dotted semantic-convention names
(with XRootD/WLCG-specific fields under the `xrootd.*`/`wlcg.*` vendor
namespaces). There is no top-level `type` field: the record kind is
`attributes["event.name"]`. This one in-memory shape is then framed differently
per wire sink.

#### OpenSearch `_bulk`

The `_bulk` framing (`XrdMonOpenSearch::Add`) emits, per document, an
action/metadata line followed by the source line:

```
{"index":{"_index":"xrootd-transfers"}}
{"resource":{…},"attributes":{"event.name":"xrootd.transfer",…}}
```

A rolling index uses the `index` action (an upsert); a **data stream** uses the
`create` action instead (`--os-datastream`; data streams reject `index`). Bodies
are POSTed to `<url>/_bulk` with `Content-Type: application/x-ndjson`. A store
that expands dotted field names (such as OpenSearch) indexes each key as a nested
field (`attributes.file.path`, `resource.server.address`); the committed
[`opensearch-template.json`](opensearch-template.json) maps these for that sink.

#### OTLP / JSON

The OTLP encoder (`XrdMonOtlp.cc`) re-encodes the nested `resource`/`attributes`
objects into the strict OTLP `resourceLogs`/`resourceSpans` envelope with typed
`KeyValue` arrays (`toKeyValues` / `toAnyValue`: 64-bit integers as strings per
the proto3-JSON mapping, nested objects/arrays as `stringValue`). Records are
grouped by resource (one group per server incarnation), and classified as
**logs** (the default) or **spans** (documents that carry a `kind`, produced with
`--spans`). Logs POST to `<url>/v1/logs`, spans to `<url>/v1/traces`; the
log/span envelope fields (severity, times, trace/span ids, name/kind/status) pass
through since they are already OTLP-shaped, and the log `body` is the record's
`event.name`:

```json
{"resourceLogs":[{
  "resource":{"attributes":[{"key":"service.name","value":{"stringValue":"xrootd"}}, …]},
  "scopeLogs":[{"scope":{"name":"xrdmoncollect"},
    "logRecords":[{"timeUnixNano":"…","severityNumber":9,
      "body":{"stringValue":"xrootd.transfer"},
      "attributes":[{"key":"file.path","value":{"stringValue":"/store/…"}}, …]}]}]}]}
```

### Durability and offline caching

When an HTTP receiver is offline or returns errors, the output thread first
**retries in place**: transient failures (a network error, HTTP 429, or any 5xx)
are retried up to four times with exponential backoff (1s, 2s, 4s, 8s; capped at
16s). If a body still cannot be delivered:

- With `--cache-dir`, it is **spooled to disk** (`XrdMonDiskCache`): written to a
  `<name>.tmp` file and then atomically renamed to
  `<13-digit-epoch-ms>-<6-digit-seq>.ndjson`. Cached bodies are replayed
  **oldest-first** once the sink recovers, and any files left by a previous run
  are replayed on **startup** (init scans the directory in lexical — i.e.
  chronological — order and discards stale `.tmp` partials). The cache has no
  size limit.
- Without `--cache-dir`, the body is **dropped and counted**.

The OpenSearch `_bulk` bodies live flat under the cache directory; the OTLP logs
and traces cache **separately**, because they replay to different endpoints:

```
<cache-dir>/
├── 1751450432000-000000.ndjson      # OpenSearch _bulk bodies
├── 1751450432500-000001.ndjson
├── otlp-logs/
│   └── 1751450433000-000000.ndjson  # OTLP /v1/logs bodies
└── otlp-traces/
    └── 1751450433200-000000.ndjson  # OTLP /v1/traces bodies
```

Health signals to watch (with `--metrics-port`):
`xrootd_collector_cache_files`/`_bytes` (current backlog),
`xrootd_collector_cache_stored_total`/`_replayed_total`, and
`xrootd_collector_dropped_bulk_total`; the OTLP sink has the analogous
`otlp_cache_*` / `otlp_dropped_total` series.

## Quick start

```sh
# Collect to a file as NDJSON
xrdmoncollect -p 9930 -o /var/log/xrootd/transfers.ndjson -v

# Post directly to an OpenSearch data stream
xrdmoncollect -p 9930 --os-url https://opensearch:9200 \
              --os-index xrootd-transfers --os-datastream \
              --os-user admin --os-pass secret

# Export to an OpenTelemetry collector / Grafana Alloy (logs, plus spans)
xrdmoncollect -p 9930 --otlp-url http://alloy:4318 --spans

# Forward NDJSON to a Logstash/Fluentd TCP input for buffering
xrdmoncollect -p 9930 --forward logstash.example.org:5044
```

## Streams and documents

By default the `f` (file-stats) stream produces a per-close document on each
file close and maintains the `xrootd_collector_active_transfers{server}` gauge
(open files in progress, from the `isXfr` snapshots and open/close records). A
file close is reported with `attributes["event.name"]` = `xrootd.transfer` and
one of two values of `attributes["xrootd.transfer.kind"]` that share an identical
schema:

- `transfer` — a **whole-file** copy: a read that covered the whole file
  (`xrootd.transfer.read_bytes + xrootd.transfer.readv_bytes >= file.size`, the
  size captured at open) or a write that completed cleanly (an upload producing
  the file).
- `access` — finer-grained or partial data access: a short read, a read whose
  open size is unknown (no matching open record), or a write cut short by a
  forced (disconnect-driven) close or an error. XRootD serves both whole-file
  copies and partial/random data access; this lets a consumer separate the two
  without recomputing coverage. The two are counted separately in
  `xrootd_collector_transfers_total` and `xrootd_collector_accesses_total`.

### Streams

Several opt-in streams add finer-grained events:

- `--sessions` enables per-session activity correlation: every file close that
  named the user is folded into a per-session rollup, and a `session` document
  (`attributes["event.name"]` = `xrootd.session`) is emitted on each client
  disconnect (`isDisc`). The session `attributes` carry running totals
  (`xrootd.session.files`, `.transfers`, `.accesses`, `.read_bytes`,
  `.write_bytes`, `.errors`, `.start_time`/`.end_time`/`.duration`) and a capped
  `xrootd.session.recent_files` list (the most recent closed files, each with
  `file.path`, `xrootd.transfer.kind`, `xrootd.operation`, `xrootd.bytes`). The
  totals cover every closed file; only the `recent_files` list is bounded, so a
  long session (a batch job opening many files in a dataset) stays
  memory-bounded. A client that hits an error and disconnects therefore yields
  one document with as much of its activity as the server reported. **Off by
  default** — when disabled no rollup is accumulated and no `session` document is
  produced, saving the per-session memory and receive-thread work for
  deployments that only consume the per-transfer/access documents.
- `--spans` additionally emits an OpenTelemetry **span** document alongside each
  concluded-operation log: a file-operation span per close or failed operation
  (spanning open → close, with `status` `STATUS_CODE_OK`/`STATUS_CODE_ERROR`) and,
  with `--sessions`, a session span (the trace root) per disconnect. Every log
  already carries a deterministic `traceId`/`spanId` (the trace keyed by the
  client session `src|stod|user`, the span by the file id), so the span document
  simply re-frames the same identity with the OTLP span fields (`name`, `kind`,
  `startTimeUnixNano`/`endTimeUnixNano`, `status`, `parentSpanId`) for a tracing
  backend. **Off by default**; like the logs it can be high volume.
- `--traces` turns each `t` (I/O trace) record into a document
  (`attributes["event.name"]` = `xrootd.read`/`xrootd.write` with
  `xrootd.io.offset`, `xrootd.io.length` and the resolved `file.path`,
  `xrootd.open`, `xrootd.close`, `xrootd.disconnect`, and `xrootd.appid`). This
  is **high volume** (one record per I/O) — enable only when the detail is
  needed. Requires `io` in the server's monitor `dest` list and the path
  dictionary (`d` stream) to resolve file names.
- `--gstream` forwards each `g` (plugin) record — from the `oss`, `pfc`,
  `throttle`, `tpc`, `http` g-streams — as a document tagged with its provider,
  embedding the plugin's JSON payload. Requires `xrootd.mongstream` on the
  server. Independently of document emission, when `--metrics-port` is set the
  `oss`, `pfc`, `tpc`, `throttle` and `http` providers are also parsed into
  aggregate metrics (see below); the cumulative providers (`oss`, `throttle`
  `io_total`, `http` counts) are converted to counter deltas. `ccm` and
  `tcpmon` are forwarded only.
- The `x` (FRM stage/migrate) and `p` (FRM purge) records are always decoded
  into an `frm` document (`attributes["event.name"]` = `xrootd.frm`;
  `xrootd.operation`, `user.name`, `file.path`, and — for purge — `file.size`)
  and counted in `xrootd_collector_frm_total{server,op}` /
  `xrootd_collector_frm_purge_bytes_total`. Emitted by a File Residency
  Manager.
- `--redirects` turns each `r` (redirect) record into a concluded-operation
  document: an `attributes["event.name"]` = `xrootd.transfer` report with
  `attributes["xrootd.transfer.kind"]` `"transfer"` and `xrootd.operation_state`
  `"Redirected"`, the triggering `xrootd.operation`, the destination
  (`xrootd.redirect.kind`, `xrootd.redirect.target.address`,
  `xrootd.redirect.target.port`), the redirected `file.path`, and the joined
  user/client attributes. A redirect concludes the operation from the
  redirector's point of view (the data server that ultimately serves the file
  emits its own `Successful`/`Failed` close). Emitted mainly by
  redirectors/managers; requires `redir` in the monitor `dest` list. Redirects
  are also counted in `xrootd_collector_redirects_total{server,kind}`
  regardless of this flag.

The `u` (user), `d` (path) and `i` (appinfo) dictionaries are always consumed:
they resolve identities and paths for the other streams, and the appinfo (`i`)
is joined to each transfer document by session descriptor (adds `xrootd.app.raw`
when the client set one).

The `=` (server identity), `T` (token) and `U` (user experiment/activity)
records are also always consumed:

- `=` (`MAPIDNT`) yields a one-off `server_ident` document
  (`attributes["event.name"]` = `xrootd.server_ident`) per server incarnation
  (its `resource`: `xrootd.server.site`, `xrootd.server.instance`,
  `xrootd.server.program`, `service.version`, `server.address`/`host.name`,
  `server.port`) and its host/site/instance are joined into every transfer
  document's `resource`. Re-sent identically each `ident` interval; the
  collector emits the document only when it changes.
- `T` (`MAPTOKN`) carries the token identity (subject, VO, role, groups). Keyed
  by the user dictid, it joins onto each transfer as `user.id`, `wlcg.vo`,
  `wlcg.role`, `wlcg.groups`, and drives
  `xrootd_collector_vo_transfers_total{server,vo}`. `wlcg.vo` comes from the
  token when present, else from the auth CGI `&o=` — but only for methods that
  can actually convey a VO (gsi with VOMS, sss, ztn, http/https); a `&o=` from
  unix/krb5/pwd/host auth is ignored rather than surfacing fake VO values.
  (For SciTokens the `T` record's own `&o=` is the token *issuer*.)
- `U` (`MAPUEAC`) carries the SciTags packet-marking flow labels (experiment
  and activity ids), joined onto transfers as
  `scitags.experiment_id`/`scitags.activity_id`.
  With `--scitags <src>` pointing at a SciTags registry (the scitags.org schema:
  a top-level `"experiments"` array of `{expId, expName, activities:[{activityId,
  activityName}]}`), those numeric ids are additionally mapped to human names —
  `scitags.experiment` and `scitags.activity`. These stand on their own (group
  by them in dashboards); they are deliberately not folded into `wlcg.vo`,
  which carries only genuine VO information. The numeric ids are always
  emitted, so the field is present with or without the registry; a
  missing/unparseable source is warned about at start-up and otherwise ignored.

  `<src>` is either a local file path or an `http(s)://` URL (e.g. the official
  `https://www.scitags.org/api.json`). A URL source is re-fetched in the
  background every `--scitags-refresh` seconds (default 3600; `0` disables) so a
  long-running collector tracks changes in the published registry; the swap is
  atomic with respect to the decode loop, and a failed re-fetch keeps the current
  registry. A URL source requires that the collector was built with libcurl.

### Output document

The per-transfer document uses the OpenTelemetry-aligned schema described under
[Serialization](#serialization): a process-level `resource` object and an
event-level `attributes` object. One object per file close, for example:

```json
{
  "resource": {
    "service.name": "xrootd",
    "service.instance.id": "srv1",
    "service.version": "5.6.1",
    "server.address": "srv1.example.org",
    "host.name": "srv1.example.org",
    "server.port": 1094,
    "xrootd.server.site": "SITE-A",
    "xrootd.server.instance": "srv1",
    "xrootd.server.id": 42,
    "xrootd.server.incarnation": 1700000000
  },
  "scope": { "name": "xrdmoncollect" },
  "@timestamp": "2026-07-02T10:00:32Z",
  "timeUnixNano": "1751450432000000000",
  "observedTimeUnixNano": "1751450432100000000",
  "severityNumber": 9, "severityText": "INFO",
  "traceId": "9f1c8b0d4e2a6f37c1a8b0d4e2a6f371",
  "spanId": "3ab4c1d2e3f40516",
  "attributes": {
    "event.name": "xrootd.transfer",
    "xrootd.transfer.kind": "transfer",
    "file.path": "/store/data/file.root",
    "file.name": "file.root",
    "file.size": 1073741824,
    "client.address": "192.0.2.17",
    "xrootd.client.host": "wn.example.org",
    "network.type": "ipv4",
    "user.name": "alice",
    "user.id": "https://issuer/sub42",
    "wlcg.vo": "atlas", "wlcg.role": "production", "wlcg.groups": "/atlas/prod",
    "xrootd.auth.method": "gsi",
    "xrootd.client.version": "v5.6.1",
    "xrootd.client.site": "client-site",
    "xrootd.app.name": "xrdcp",
    "xrootd.operation": "read",
    "xrootd.operation_state": "Successful",
    "xrootd.transfer.start_time": "2026-07-02T09:55:32Z",
    "xrootd.transfer.duration": 300,
    "xrootd.transfer.read_bytes": 1073741824,
    "xrootd.transfer.read_ops": 320,
    "xrootd.transfer.is_local": true
  }
}
```

`xrootd.transfer.open_seen` is `false` (and the `file.*`, `user.*`, `client.*`
attributes are absent) for a close whose open record was lost or predates the
collector — the `xrootd.transfer.*` byte totals are still reported. Empty/zero
fields are omitted, so a given document only carries what the server actually
reported.

### WLCG field mapping

The schema covers the WLCG transfer-monitoring fields that XRootD currently puts
on the wire. Mapping (and the server config each needs):

| WLCG field | XRootD field | Source / requires |
| :-- | :-- | :-- |
| file_name | `file.path` | `fstat … lfn` |
| operation_type | `xrootd.operation` (`read`/`write`) | `fstat … xfr` |
| operation_state | `xrootd.operation_state` (`Successful`/`Failed`/`Redirected`) | `fstat` (terminal report); `Redirected` from `r` with `--redirects` |
| error_message | `error.message` | `fstat` (failed open / I/O / close) |
| error_category | `error.type` + `xrootd.error.code` | `fstat` (failed open / I/O / close) |
| server_name/site | `server.address` / `xrootd.server.site` | `=` ident (`all.sitename`/`XRDSITE` for site) |
| server_ip / hostname | `server.address` / `host.name` | UDP source / `=` ident (loopback → public address / local FQDN) |
| client_ip / hostname | `client.address` / `xrootd.client.host` | login CGI `&a=` (numeric IP, 6.x+) / `u` descriptor (server DNS config) |
| client_version | `xrootd.client.version` | login appinfo (`&R=`) |
| ip_version | `network.type` (`ipv4`/`ipv6`) | login appinfo (`&I=`) |
| client_site | `xrootd.client.site` | login appinfo (`&S=`, client `XRDSITE`/`XRD_SITE`) |
| auth_method | `xrootd.auth.method` | **`… auth`** |
| user | `user.name` / `user.id` | `u` / `T` token |
| vo | `wlcg.vo` | `T` token, else `… auth` (`&o=` from a VO-bearing method: gsi/sss/ztn/http(s)) |
| activity | `scitags.experiment`/`scitags.activity` (names), `scitags.*_id` (numeric), `wlcg.role` | `U` SciTags + `--scitags` registry; `T` token for role |
| start_time / end_time | `xrootd.transfer.start_time` / `.end_time` | f-stream `FileTOD` window |
| bytes | `xrootd.transfer.{read,readv,write}_bytes` | `fstat … xfr` |
| is_local (LAN/WAN) | `xrootd.transfer.is_local` | derived: client vs server domain (needs `=` ident) |

`client.address` is the numeric client IP the server puts in the login CGI
(`&a=`, XRootD 6.x+; never a DNS name). Against an older server it falls back
to the `u` descriptor's host, which may be a reverse-resolved name depending
on the server's DNS configuration. When the server reverse-resolved the client
to a hostname, that name is kept in `xrootd.client.host` (omitted when it
would just repeat `client.address`).

`host.name` precedence is: the host advertised on the `=` ident stream
(when it is a real name, not an IP literal; a `localhost*` name is replaced by
the collector's own FQDN), else — for a server reporting from the loopback
address (the common co-located collector + server setup, where the UDP source
is `::1`/`127.0.0.1`) — the collector's own local FQDN, since the reporting
server runs on the same host. `server.address` falls back to the numeric IP
(it carries the IP when no hostname is available).

Loopback literals (`127.0.0.0/8`, `::1`) that would otherwise be emitted in
`client.address`/`server.address` are replaced with this host's public
address: at startup (never in the receive loop) the collector resolves its own
FQDN once and caches the first non-loopback IPv4 and IPv6 address; when name
resolution yields nothing usable (e.g. `/etc/hosts` pinning the host name to
`127.0.1.1`), the first public — else private — interface address of each
family is used instead. A loopback of one family is replaced by the cached
address of the same family, falling back to the other family, then to the
FQDN. `--no-resolve` disables all of these substitutions (leaving the numeric
addresses and names as received). A *remote* server is never reverse-resolved
here — that hostname comes from its `=` ident, and a blocking reverse-DNS
lookup of an arbitrary source IP would stall the UDP receive loop.

`xrootd.transfer.is_local` is a heuristic: it is `true` when the client
(`xrootd.client.host`, falling back to `client.address`) and the reporting
server share a registered domain (the part after the first host label),
`false` when they differ, and **omitted** when either side is an IP literal or
the server host is unknown (no `=` ident yet). It also drives
`xrootd_collector_locality_transfers_total{server,locality}`.

`xrootd.operation_state` is the authoritative success/failure of the
operation: a plain close reports `Successful`, while a failed open, a
mid-transfer read/write error, or a failed close reports `Failed` together with
`error.type` (`open`/`read`/`write`/`close`/`auth`),
`xrootd.error.code` (the XRootD error code), and `error.message`. A
failed open never produced any close record before; the server now emits a
terminal `isError` f-stream record, and sets `hasERR` on the close for a failed
close or a terminal `read`/`readv`/`pgread`/`write`/`writev`/`pgwrite` error
recorded during the session (the common "readv past EOF" surfaces as a `read`
failure). The `isError` record covers open failures reported synchronously
(`fsError`), asynchronously (the deferred-open callback), and the early `do_Open`
denials that bypass `fsError` — notably a second writer rejected with
`kXR_FileLocked`. All are keyed on
`xrootd_collector_failed_operations_total{server,category}`. This requires only
the existing `fstat` setup — no extra directive. A disconnect-driven
(`xrootd.transfer.forced_close`) close is **not** a failure unless an error was
actually recorded. The `error.message` is the server's own SFS reason verbatim
(e.g. `Unable to open …; no such file or directory` for a missing file); the
`XRootD::moncollect` test asserts that specific reason per-document for both a
failed open and the readv-past-EOF case.

A third terminal state, `"Redirected"`, is reported for `r`-stream redirect
records (with `--redirects`): from the redirector's point of view the operation
concluded by sending the client elsewhere. The redirect destination travels as
`xrootd.redirect.kind`, `xrootd.redirect.target.address`, and
`xrootd.redirect.target.port`; the data server that ultimately serves the file
emits its own `Successful`/`Failed` close.

`xrootd.client.site` is the site the *client* advertises for itself: an XRootD
client that has `XRDSITE` (or `XRD_SITE`, which takes precedence) set in its
environment sends it in the login CGI (`xrd.site`), the server folds it into the
user-map appinfo (`&S=`), and the collector surfaces it as `xrootd.client.site`.
It is absent when the client does not advertise one. (This is distinct from
`xrootd.server.site`, which is the *reporting server's* site — `all.sitename`
in its config, exported as `XRDSITE` — from the `=` ident record. A site that
is only dots — `XrdOucSiteName`'s sanitization of an entirely invalid name,
e.g. a stray `XRDSITE` env var inherited by a daemon with no `all.sitename`
directive — is dropped as carrying no information.)

## Sinks

Documents fan out to any combination of sinks; stdout is used only as the
fallback when no other sink is configured (`-o` always adds a file too):

- **File / stdout** (`-o`, `--bulk`): NDJSON, or the OpenSearch `_bulk` framing
  with `--bulk <index>`. Ship it with an external agent (Filebeat) or `curl`.
- **OpenSearch** (`--os-url`): available when the binary is built with libcurl
  (the build links `CURL::libcurl` if found). Documents are batched and posted
  via the `_bulk` API on a dedicated output thread; transient failures (network,
  HTTP 429/5xx) are retried with exponential backoff. With `--cache-dir` a body
  that still fails is written to disk and retried later (see
  [Durability and offline caching](#durability-and-offline-caching)).
- **OTLP/HTTP** (`--otlp-url`): posts the documents to an OpenTelemetry endpoint
  as OTLP/JSON — logs to `<url>/v1/logs` and, with `--spans`, spans to
  `<url>/v1/traces` — so xrdmoncollect feeds an **OpenTelemetry Collector** or
  **Grafana Alloy** natively; the collector then routes to Loki, Tempo,
  Elasticsearch, Kafka, and so on. The nested `resource`/`attributes` objects are
  re-encoded into the strict OTLP `resourceLogs`/`resourceSpans` envelope with
  typed `KeyValue` attributes (see [Serialization](#serialization)). Batched per
  flush on a dedicated output thread with retry/backoff. With `--cache-dir` a body
  that still fails is written to disk and retried later (logs and traces cache
  separately under `otlp-logs`/`otlp-traces` subdirectories, since they replay to
  different endpoints); without it a terminal failure drops the body (counted).
  Requires libcurl; `--otlp-insecure` skips TLS verification. This is the
  log/trace analogue of the metrics OTLP push in `XrdHttpMetricsExporter`.
- **TCP forward** (`--forward host:port`): streams the same NDJSON over a plain
  TCP connection to a buffering/forwarding frontend — Logstash (`tcp` input),
  Fluentd (`in_tcp`), Vector (`socket` source), or a message-broker bridge. The
  connection is lazily (re)established with a short cool-down; documents
  produced while the consumer is down are dropped (durable buffering is the
  downstream's job). Dependency-free, so it is built even without libcurl.

### Authentication

Both HTTP sinks authenticate to their endpoint:

- **Basic auth** (OpenSearch only): `--os-user`/`--os-pass`, sent as
  `Authorization: Basic`.
- **Bearer token** (both sinks): `--os-token` / `--otlp-token`, sent as
  `Authorization: Bearer <token>` on every request. This is the usual scheme for
  OTLP collectors (Grafana Alloy, a gateway OTel Collector) and for token-secured
  OpenSearch. For OpenSearch a bearer token takes precedence over basic auth if
  both are configured (they share the `Authorization` header).

A token given as `@<file>` is read from that file (trailing whitespace stripped)
instead of being taken literally, so the secret stays out of `ps`/argv; a
config-file value (a `[xrdmoncollect]` `os-token`/`otlp-token` key in a
mode-`0600` file) works the same way. The TCP `--forward` sink is a plain socket
with no application-layer auth — put it behind a trusted network or a TLS proxy.

## Configuration

### Command-line options

```
xrdmoncollect -p <port> [-b <bindaddr>] [-o <file>] [--bulk <index>]
              [--os-url <url> [--os-index <name>] [--os-user <u>]
               [--os-pass <p>] [--os-token <t>] [--os-insecure] [--os-datastream]]
              [--otlp-url <url> [--otlp-token <t>] [--otlp-insecure]]
              [--forward <host:port>]
              [--flush-count <n>] [--flush-secs <n>] [--dump] [-v]

  -c <file>        load options from an INI config file (see Configuration)
  -p <port>        UDP port to listen on (required)
  -b <bindaddr>    address to bind (default: all interfaces, dual-stack)
  -o <file>        append output to <file> (default: stdout unless a network sink)
  --bulk <index>   write OpenSearch _bulk format to the file/stdout sink
  --os-url <url>   POST documents to an OpenSearch cluster's _bulk API
  --os-index <n>   index/data-stream name (default: xrootd-transfers)
  --os-user <u>    basic-auth user
  --os-pass <p>    basic-auth password
  --os-token <t>   bearer token (Authorization: Bearer); wins over basic auth;
                   @<file> reads the token from a file
  --os-insecure    skip TLS certificate verification
  --os-datastream  target is a data stream (use the "create" bulk action)
  --otlp-url <url> POST OTLP/JSON to an OTel collector (logs -> /v1/logs,
                   spans -> /v1/traces with --spans)
  --otlp-token <t> bearer token (Authorization: Bearer); @<file> reads it from
                   a file
  --otlp-insecure  skip TLS verification for the OTLP endpoint
  --cache-dir <d>  cache _bulk bodies that fail to POST under <d> and retry them
                   (oldest-first, replayed on startup; default: off = drop)
  --forward <h:p>  also stream documents as NDJSON over TCP to host:port
  --flush-count <n> packets per receive batch / one batch -> one POST (def 500)
  --flush-secs <n>  hand off a partial batch after N seconds (default: 5)
  --rcvbuf <sz>     kernel UDP receive buffer, SO_RCVBUF (K/M/G; default 16M)
  --queue-depth <n> receive->serialize batches in flight (default: 64)
  --metrics-port <p> serve aggregated metrics over HTTP on port <p>
  --max-memory <sz>  bound correlation state to ~<sz> bytes, LRU-evicting
                     (K/M/G suffix; default 256M; 0=unbounded)
  --max-entries <n>  optional hard cap on correlation entries (0=off)
  --server-ttl <s>   reclaim a server incarnation idle for >s seconds
                     (default 86400; 0=never)
  --scitags <src>  SciTags registry (file path or http(s):// URL) mapping
                   experiment/activity ids to names
  --scitags-refresh <s> re-fetch a URL registry every <s> seconds (default 3600)
  --no-resolve     do not substitute the local FQDN / public address for
                   loopback addresses and localhost names
  --sessions       correlate per-session activity and emit a session document
                   per client disconnect (off by default)
  --spans          also emit OpenTelemetry span documents alongside the logs
  --traces         emit a document per t-stream I/O record (high volume)
  --gstream        emit a document per g-stream (plugin) record
  --redirects      emit a document per r-stream redirect record
  --dump           also emit one JSON object per decoded record (debugging)
  -v               print decoder statistics on exit (SIGINT/SIGTERM)
```

### Configuration file

Options may be set in an INI configuration file instead of (or in addition to)
the command line. The file is loaded from the path given with `-c`/`--config`,
or automatically from `/etc/xrootd/xrdmoncollect.cfg` when that file exists (a
missing default path is silently ignored). All keys live in a single
`[xrdmoncollect]` section and mirror the long-option names; **command-line
options override values from the file**. A named-but-unreadable or malformed
file is a fatal error. See `xrdmoncollect.cfg.example` for the full key list.

```ini
[xrdmoncollect]
port = 9930
os-url = https://opensearch.example.org:9200
os-index = xrootd-transfers
forward = logstash.example.org:5044
metrics-port = 9931
max-memory = 256M
```

### Server configuration

Point the server's file-stats stream at the collector. **The `xfr` option is
required to get close records** (and therefore transfer documents): without it
the server registers opens but never emits the per-file `isClose` record
(`XrdXrootdMonFile.cc` only assigns the monitor entry when I/O stats are kept).
The `lfn` option adds the path to the open record and `ops`/`ssq` add the
operation counts and sum-of-squares to the close record. The `auth` option
enriches the user dictionary with the authentication method and VO (see the
field table above); without it those fields are simply absent.

The VO path (gsi → VOMS attribute certificate → `XrdSecEntity.vorg` → MAPUSER
`&o=` → `wlcg.vo`) is exercised end-to-end by the `XRootD::moncollect`
integration test when the VOMS plug-in is built: it mints a fake VOMS proxy with
`voms-proxy-fake` and asserts `wlcg.vo` appears on the transfer document.

```
xrootd.monitor all flush 30 fstat 30 lfn ops ssq xfr 1 auth \
               dest fstat info user <collector-host>:9930
```

## Deployment and tuning

### Running as a service

A systemd unit, `xrdmoncollect.service`, runs the collector as the `xrootd`
user reading `/etc/xrootd/xrdmoncollect.cfg`:

```sh
# edit /etc/xrootd/xrdmoncollect.cfg, then:
systemctl enable --now xrdmoncollect
journalctl -u xrdmoncollect -f
```

The unit is non-templated (one collector per host). Extra environment can be
supplied via `/etc/default/xrdmoncollect` or `/etc/sysconfig/xrdmoncollect`. The
default UDP port (9930) is unprivileged; for a port below 1024 add
`CAP_NET_BIND_SERVICE` to the unit's `CapabilityBoundingSet`/`AmbientCapabilities`.

The collector can run **co-located** with a server (the common case: the server
reports from the loopback address and the collector substitutes its own FQDN for
`host.name`, see [WLCG field mapping](#wlcg-field-mapping)) or as a **central**
receiver for many servers, each pointed at `<collector-host>:9930`.

### Capacity and tuning

Enable `--metrics-port` and watch the collector's own metrics to size the knobs.
The defaults suit a busy single server; a central collector for many servers may
need larger buffers and a bigger memory budget.

| Symptom (metric) | Knob(s) | Action |
| :-- | :-- | :-- |
| `recv_queue_batches` rides at `--queue-depth`; `packets_lost_total` climbs | `--rcvbuf`, `--queue-depth` | Enlarge the kernel socket buffer and/or the in-flight batch queue so bursts are absorbed instead of dropped. |
| Too many small POSTs, or POST latency too high | `--flush-count`, `--flush-secs` | Larger/longer flush windows trade freshness for fewer, bigger requests; smaller windows lower end-to-end latency. |
| `state_bytes` near `--max-memory`; `evicted_total` climbing | `--max-memory`, `--max-entries`, `--server-ttl` | Raise the budget to cover the working set (≈ concurrent open files × incarnations); shorten the TTL to reclaim dead incarnations sooner. |
| `post_failures_total` / `otlp_failures_total`, growing `cache_files` | `--cache-dir` (+ downstream) | Ensure a cache dir is set so failures spool to disk instead of dropping; investigate the sink. Watch `dropped_bulk_total` for actual loss. |

Backpressure is intentional: if a sink stalls, the POST queue fills, the
serializer slows, and finally the receiver relies on `--rcvbuf` to ride out the
gap. Size `--rcvbuf` and `--cache-dir` for the longest sink outage you must
survive without loss.

## Consuming the data

### OpenSearch index / data stream

`--os-index` names either a rolling index or a **data stream**. A data stream is
the recommended shape for this append-only, time-series data: pass
`--os-datastream` so the collector uses the `_bulk` `create` action (data
streams reject `index`) and relies on the `@timestamp` every document carries.

A composable index template with an explicit mapping for the dotted
semantic-convention field names (the `resource.*`, `attributes.*`,
`xrootd.*`, and `wlcg.*` keys) is provided in
[`opensearch-template.json`](opensearch-template.json)
(IPs as `ip`, byte counters as `long`, identifiers as `keyword`, strings mapped
to `keyword` by default rather than analyzed `text`). Apply it once before
ingesting; it also creates the data stream backing the `xrootd-transfers` name:

```sh
curl -s -H 'Content-Type: application/json' \
     -XPUT https://opensearch:9200/_index_template/xrootd-transfers \
     --data-binary @opensearch-template.json
```

Drop the `data_stream` block from the template (and omit `--os-datastream`) to
use a plain rolling index with an ISM rollover policy instead.

### OpenSearch Dashboards

A ready-to-import OpenSearch Dashboards saved-objects file is provided in
[`opensearch-dashboards.ndjson`](opensearch-dashboards.ndjson): an
`xrootd-transfers*` index pattern plus a *XRootD Transfers (xrdmoncollect)*
dashboard built on the log records — throughput over time, transfer/access
rates, VO / auth-method / locality breakdowns, error categories, transfer
duration distribution, and top files/users/sites. Import it under **Dashboards
Management → Saved Objects → Import** (or via the API):

```sh
curl -s -u admin:secret -H 'osd-xsrf: true' \
     -XPOST 'https://dashboards:5601/api/saved_objects/_import?overwrite=true' \
     --form file=@opensearch-dashboards.ndjson
```

Traces (the `--spans` OTLP stream) are not shown here: their natural home is the
tracing backend the OTLP collector feeds (e.g. Tempo or Jaeger), which render
the `traceId`/`spanId` correlation as trace waterfalls.

### Loki / Grafana

The same log records can drive a Grafana dashboard backed by
[Grafana Loki](https://grafana.com/oss/loki/) instead of OpenSearch. Point the
OTLP sink at Loki's OTLP endpoint (optionally through an OpenTelemetry Collector
or Grafana Alloy in between):

```sh
xrdmoncollect -p 9930 --otlp-url http://loki:3100/otlp
```

`xrdmoncollect` appends `/v1/logs` to the OTLP URL, matching Loki's OTLP ingest
path. This requires **Loki ≥ 3.0** with structured metadata and OTLP ingestion
enabled (both on by default in 3.x). Loki promotes only a small set of *resource*
attributes to stream labels — for our records `service.name` (always `xrootd`)
and `service.instance.id` — and stores everything else, including all event
attributes, as **structured metadata** with dots rewritten to underscores. So the
OpenSearch field `attributes.xrootd.transfer.kind` becomes the queryable label
`xrootd_transfer_kind`, and a typical query reads:

```logql
sum by (xrootd_transfer_kind) (
  count_over_time({service_name="xrootd"} | event_name="xrootd.transfer" [$__auto])
)
```

A ready-to-import dashboard is provided in
[`grafana-loki-dashboard.json`](grafana-loki-dashboard.json) — the same panels as
the OpenSearch dashboard (throughput, rate by kind, VO / auth / locality / state
breakdowns, error categories, top files/users/sites, sessions). Import it under
**Grafana → Dashboards → New → Import** and select your Loki data source for the
`DS_LOKI` input. Two panels are approximations, because LogQL lacks the matching
aggregation: *Distinct clients* (no native count-distinct) and *Transfer duration
quantiles* (p50/p90/p99, standing in for OpenSearch's fixed-bucket duration
histogram).

### Aggregated metrics (Prometheus)

With `--metrics-port <p>` the collector also runs a small HTTP exporter that
serves Prometheus metrics aggregated from the decoded transfers. Unlike the
per-transfer documents (which belong in a document store), these are bounded
in cardinality — labelled only by the reporting `server` — and suitable for a
time-series database:

```
xrootd_collector_transfers_total{server="..."}   (whole-file closes)
xrootd_collector_accesses_total{server="..."}    (partial-access closes)
xrootd_collector_read_bytes_total{server="..."}
xrootd_collector_write_bytes_total{server="..."}
xrootd_collector_vo_transfers_total{server="...",vo="..."}
xrootd_collector_locality_transfers_total{server="...",locality="local|remote"}
xrootd_collector_sessions_total{server="..."}
xrootd_collector_active_transfers{server="..."}   (gauge)
xrootd_collector_transfer_size_bytes        (histogram)
xrootd_collector_transfer_duration_seconds  (histogram)
xrootd_collector_packets_total              (and other decoder statistics)
xrootd_collector_recv_queue_batches         (gauge: receiver->serializer depth)
xrootd_collector_post_queue_bodies          (gauge: bodies awaiting the POST)
xrootd_collector_post_failures_total        (OpenSearch _bulk POST failures)
xrootd_collector_cache_files                (gauge: cached bodies awaiting replay)
xrootd_collector_cache_bytes                (gauge: bytes of cached bodies)
xrootd_collector_cache_stored_total         (bodies written to the disk cache)
xrootd_collector_cache_replayed_total       (cached bodies replayed)
xrootd_collector_dropped_bulk_total         (bodies dropped: no/failed cache)
```

From the `g` (plugin) streams (when `--gstream` data is flowing):

```
xrootd_collector_oss_ops_total{server="...",op="..."}
xrootd_collector_oss_slow_ops_total{server="...",op="..."}
xrootd_collector_pfc_files_total{server="..."}
xrootd_collector_pfc_bytes_total{server="...",source="hit|miss|bypass|disk|prefetch"}
xrootd_collector_tpc_total{server="...",type="push|pull",result="ok|error"}
xrootd_collector_tpc_bytes_total{server="...",type="push|pull"}
xrootd_collector_tpc_size_bytes             (histogram)
xrootd_collector_throttle_io_total{server="..."}
xrootd_collector_throttle_io_active{server="..."}   (gauge)
xrootd_collector_http_requests_total{server="...",method="...",status="..."}
```

From the `x`/`p` (FRM) streams:

```
xrootd_collector_frm_total{server="...",op="transfer|purge"}
xrootd_collector_frm_purge_bytes_total{server="..."}
```

Point a Prometheus scrape job at `http://<collector-host>:<p>/metrics`.

[`grafana-dashboard.json`](grafana-dashboard.json) (next to this README) is a
ready-to-import dashboard covering the metrics above: collector health (ingest/decode rates, correlation
memory, queue depth), sink health (POST failures, drops, queue and disk-cache
backlog for both the OpenSearch and OTLP sinks), transfer activity per server
(throughput, active transfers, failed operations, duration/size histogram
quantiles, VO and locality breakdowns), and the `g`/`x`/`p`-stream backends
(redirects, TPC, PFC, OSS, HTTP, throttle, FRM). Import it in Grafana
(*Dashboards → New → Import*), then pick the Prometheus data source that scrapes
the collector; a **Server** variable multi-selects the reporting servers.

## Limitations

- Correlation state is bounded by `--max-memory` / `--max-entries` (see
  [Correlation state](#correlation-state)). A dropped entry merely yields a
  document missing that field, or an orphan close. Evictions are counted in
  `xrootd_collector_evicted_total` and the live budget utilisation is the
  `xrootd_collector_state_bytes` gauge; reclaimed incarnations are counted in
  `xrootd_collector_reaped_servers_total`.
- UDP is lossy: a lost open record yields an orphan close; a lost dictionary
  record yields a document without identity/path. The server stamps every
  datagram to one destination with a single sequence number (header `pseq`), so
  the collector estimates loss from forward gaps in it —
  `xrootd_collector_packets_lost_total{server}` and the `-v` `lost=` count.
  (Reordering, a small backward step, is not counted as loss.)
- Only the `f` stream is correlated today. The `t` (per-I/O trace) and `g`
  (plugin) streams are decoded enough to be counted and optionally emitted as
  documents, but are not joined into the transfer correlation.

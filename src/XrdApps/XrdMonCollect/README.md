# xrdmoncollect

`xrdmoncollect` reads XRootD detailed-monitoring UDP packets (the
`xrootd.monitor` streams), correlates the **`f` (file-stats) stream** against
the user dictionary, and writes **one JSON document per completed transfer**
(file close). The output is line-delimited JSON (NDJSON), or the OpenSearch
`_bulk` format, suitable for ingestion into OpenSearch / Elasticsearch.

This is the document-oriented half of the XRootD monitoring story; an aggregate
Prometheus sink (counters/histograms over bounded dimensions) is a planned
addition. See `xrootd-new-metrics.md`, Phase 5.

## Usage

```
xrdmoncollect -p <port> [-b <bindaddr>] [-o <file>] [--bulk <index>]
              [--os-url <url> [--os-index <name>] [--os-user <u>]
               [--os-pass <p>] [--os-insecure] [--os-datastream]]
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
  --os-insecure    skip TLS certificate verification
  --os-datastream  target is a data stream (use the "create" bulk action)
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
  --no-resolve     do not substitute the local FQDN for a loopback server
  --sessions       correlate per-session activity and emit a session document
                   per client disconnect (off by default)
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

All document types — `transfer`, `access`, `session`, `server_ident`,
`frm`, `redirect`, the `t`-stream traces and `gstream` — share one nested,
OpenSearch-friendly schema (`server.*`, `client.*`, `user.*`, `file.*`,
`transfer.*`, …; each nested object indexes as a dotted field).

A file close is reported as one of two types that share an identical schema:

- `transfer` — a **whole-file** copy: a read that covered the whole file
  (`transfer.read_bytes + transfer.readv_bytes >= file.size`, the size captured
  at open) or a write that completed cleanly (an upload producing the file).
- `access` — finer-grained or partial data access: a short read, a read whose
  open size is unknown (no matching open record), or a write cut short by a
  forced (disconnect-driven) close or an error. XRootD serves both whole-file
  copies and partial/random data access; this lets a consumer separate the two
  without recomputing coverage. The two are counted separately in
  `xrootd_collector_transfers_total` and `xrootd_collector_accesses_total`.

### Streams

By default the `f` (file-stats) stream produces a per-close document on each
file close (`transfer` or `access`, above) and maintains the
`xrootd_collector_active_transfers{server}` gauge (open files in progress, from
the `isXfr` snapshots and open/close records). Several opt-in streams add
finer-grained events:

- `--sessions` enables per-session activity correlation: every file close that
  named the user is folded into a per-session rollup, and a `session` document
  is emitted on each client disconnect (`isDisc`). The `session` object carries
  running totals (`files`, `transfers`, `accesses`, `read_bytes`, `write_bytes`,
  `errors`, `start_time`/`end_time`/`duration_s`) and a capped `recent_files`
  list (the most recent closed files, each with `lfn`, `type`, `operation`,
  `bytes`). The totals cover every closed file; only the `recent_files` list is
  bounded, so a long session (a batch job opening many files in a dataset) stays
  memory-bounded. A client that hits an error and disconnects therefore yields
  one document with as much of its activity as the server reported. **Off by
  default** — when disabled no rollup is accumulated and no `session` document is
  produced, saving the per-session memory and receive-thread work for
  deployments that only consume the per-transfer/access documents.
- `--traces` turns each `t` (I/O trace) record into a document: `read`/`write`
  (with offset, length and the resolved `file.lfn`), `open`, `close`,
  `disconnect`, and `appid`. This is **high volume** (one record per I/O) —
  enable only when the detail is needed. Requires `io` in the server's monitor
  `dest` list and the path dictionary (`d` stream) to resolve file names.
- `--gstream` forwards each `g` (plugin) record — from the `oss`, `pfc`,
  `throttle`, `tpc`, `http` g-streams — as a document tagged with its provider,
  embedding the plugin's JSON payload. Requires `xrootd.mongstream` on the
  server. Independently of document emission, when `--metrics-port` is set the
  `oss`, `pfc`, `tpc`, `throttle` and `http` providers are also parsed into
  aggregate metrics (see below); the cumulative providers (`oss`, `throttle`
  `io_total`, `http` counts) are converted to counter deltas. `ccm` and
  `tcpmon` are forwarded only.
- The `x` (FRM stage/migrate) and `p` (FRM purge) records are always decoded
  into an `frm` document (operation, `user.name`, `file.lfn`, and — for purge —
  `file.size`) and counted in `xrootd_collector_frm_total{server,op}` /
  `xrootd_collector_frm_purge_bytes_total`. Emitted by a File Residency
  Manager.
- `--redirects` turns each `r` (redirect) record into a concluded-operation
  document: a `type:"transfer"` report with `transfer.operation_state`
  `"Redirected"`, the triggering `transfer.operation`, the destination under
  `redirect` (`kind`, `target_host`, `target_port`), the redirected `file.lfn`,
  and the joined `user`/`client`. A redirect concludes the operation from the
  redirector's point of view (the data server that ultimately serves the file
  emits its own `Successful`/`Failed` close). Emitted mainly by
  redirectors/managers; requires `redir` in the monitor `dest` list. Redirects
  are also counted in `xrootd_collector_redirects_total{server,kind}`
  regardless of this flag.

The `u` (user), `d` (path) and `i` (appinfo) dictionaries are always consumed:
they resolve identities and paths for the other streams, and the appinfo (`i`)
is joined to each transfer document by session descriptor (adds `app.raw`
when the client set one).

The `=` (server identity), `T` (token) and `U` (user experiment/activity)
records are also always consumed:

- `=` (`MAPIDNT`) yields a one-off `server_ident` document per server
  incarnation (`server.{site,hostname,instance,program,version,port}`) and its
  host/site/instance are joined into every transfer document's `server` object.
  Re-sent identically each `ident` interval; the collector emits the document
  only when it changes.
- `T` (`MAPTOKN`) carries the token identity (subject, VO, role, groups). Keyed
  by the user dictid, it joins onto each transfer as
  `user.{subject,vo,role,groups}`, and drives
  `xrootd_collector_vo_transfers_total{server,vo}`.
- `U` (`MAPUEAC`) carries the SciTags packet-marking flow labels (experiment
  and activity ids), joined onto transfers as `activity.{experiment_id,activity_id}`.
  With `--scitags <src>` pointing at a SciTags registry (the scitags.org schema:
  a top-level `"experiments"` array of `{expId, expName, activities:[{activityId,
  activityName}]}`), those numeric ids are additionally mapped to human names —
  `activity.experiment` and `activity.activity` — and the experiment name is used
  as a `user.vo` fallback (only when neither the `T` token nor the auth CGI `&o=`
  supplied a VO). The numeric ids are always emitted, so the field is present with
  or without the registry; a missing/unparseable source is warned about at
  start-up and otherwise ignored.

  `<src>` is either a local file path or an `http(s)://` URL (e.g. the official
  `https://www.scitags.org/api.json`). A URL source is re-fetched in the
  background every `--scitags-refresh` seconds (default 3600; `0` disables) so a
  long-running collector tracks changes in the published registry; the swap is
  atomic with respect to the decode loop, and a failed re-fetch keeps the current
  registry. A URL source requires that the collector was built with libcurl.

### Sinks

Documents fan out to any combination of sinks; stdout is used only as the
fallback when no other sink is configured (`-o` always adds a file too):

- **File / stdout** (`-o`, `--bulk`): NDJSON, or the OpenSearch `_bulk` framing
  with `--bulk <index>`. Ship it with an external agent (Filebeat) or `curl`.
- **OpenSearch** (`--os-url`): available when the binary is built with libcurl
  (the build links `CURL::libcurl` if found). Documents are batched and posted
  via the `_bulk` API on a dedicated output thread; transient failures (network,
  HTTP 429/5xx) are retried with exponential backoff. With `--cache-dir` a body
  that still fails is written to disk and retried later (see *Pipeline and
  durability*).
- **TCP forward** (`--forward host:port`): streams the same NDJSON over a plain
  TCP connection to a buffering/forwarding frontend — Logstash (`tcp` input),
  Fluentd (`in_tcp`), Vector (`socket` source), or a message-broker bridge. The
  connection is lazily (re)established with a short cool-down; documents
  produced while the consumer is down are dropped (durable buffering is the
  downstream's job). Dependency-free, so it is built even without libcurl.

### Pipeline and durability

The collector is a three-stage pipeline so a slow or unreachable sink never
costs UDP packets:

1. **Receiver** (main thread) does nothing but drain the socket into pooled
   packet batches and hand them to the serializer through a bounded recycling
   queue. The kernel receive buffer is enlarged (`--rcvbuf`, default 16M) and the
   queue is generously sized (`--queue-depth`, default 64). To prioritise not
   losing packets the receiver applies **backpressure** (it waits for a free
   batch) rather than dropping; combined with the large socket buffer it
   effectively never has to wait.
2. **Serializer** owns the decoder, correlates each packet, writes the file and
   forward sinks inline, and builds one OpenSearch `_bulk` body per batch
   (`--flush-count` packets / `--flush-secs`).
3. **Output** thread performs the (blocking) `_bulk` POST, so neither decoding
   nor reception ever waits on OpenSearch.

When `--cache-dir` is set, a body that still fails to POST after retries is
written there (`<epoch_ms>-<seq>.ndjson`, via a `.tmp` partial + atomic rename)
and retried oldest-first once the sink recovers; files left by a previous run are
replayed on startup. The cache has no size limit. Without `--cache-dir`, a body
that cannot be posted is dropped (counted in `xrootd_collector_dropped_bulk_total`).
The `xrootd_collector_cache_files`/`_bytes` gauges show the current backlog.

#### Index vs data stream

`--os-index` names either a rolling index or a **data stream**. A data stream is
the recommended shape for this append-only, time-series data: pass
`--os-datastream` so the collector uses the `_bulk` `create` action (data
streams reject `index`) and relies on the `@timestamp` every document carries.

A composable index template with an explicit, ECS-style mapping for the dotted
field names is provided in [`opensearch-template.json`](opensearch-template.json)
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

### Examples

```sh
# Collect to a file as NDJSON
xrdmoncollect -p 9930 -o /var/log/xrootd/transfers.ndjson -v

# Post directly to an OpenSearch data stream
xrdmoncollect -p 9930 --os-url https://opensearch:9200 \
              --os-index xrootd-transfers --os-datastream \
              --os-user admin --os-pass secret

# Forward NDJSON to a Logstash/Fluentd TCP input for buffering
xrdmoncollect -p 9930 --forward logstash.example.org:5044

# Produce an OpenSearch bulk file and ship it manually
xrdmoncollect -p 9930 --bulk xrootd-transfers -o /tmp/bulk.ndjson
curl -s -H 'Content-Type: application/x-ndjson' \
     -XPOST https://opensearch:9200/_bulk --data-binary @/tmp/bulk.ndjson
```

## Server configuration

Point the server's file-stats stream at the collector. **The `xfr` option is
required to get close records** (and therefore transfer documents): without it
the server registers opens but never emits the per-file `isClose` record
(`XrdXrootdMonFile.cc` only assigns the monitor entry when I/O stats are kept).
The `lfn` option adds the path to the open record and `ops`/`ssq` add the
operation counts and sum-of-squares to the close record. The `auth` option
enriches the user dictionary with the authentication method and VO (see the
field table below); without it those fields are simply absent.

The VO path (gsi → VOMS attribute certificate → `XrdSecEntity.vorg` → MAPUSER
`&o=` → `user.vo`) is exercised end-to-end by the `XRootD::moncollect`
integration test when the VOMS plug-in is built: it mints a fake VOMS proxy with
`voms-proxy-fake` and asserts `user.vo` appears on the transfer document.

```
xrootd.monitor all flush 30 fstat 30 lfn ops ssq xfr 1 auth \
               dest fstat info user <collector-host>:9930
```

## Output document

The per-transfer document uses an OpenSearch-friendly nested schema: each nested
object indexes as a dotted field (`server.name`, `client.ip`, `transfer.read_bytes`).
One object per file close, for example:

```json
{
  "type": "transfer",
  "@timestamp": "2026-06-23T15:57:53Z",
  "server": { "name": "srv.example.org", "ip": "::1", "hostname": "srv.example.org",
              "site": "T1_DE_KIT", "instance": "manager",
              "id": 53605690318209, "start": 1782230264 },
  "client": { "host": "wn42.example.org", "hostname": "wn42.example.org",
              "version": "v5.6.1", "ip_version": 4, "site": "T2_DE_DESY" },
  "user":   { "name": "amadio", "protocol": "xroot", "auth_method": "gsi",
              "vo": "atlas", "role": "production", "subject": "https://issuer/sub42" },
  "file":   { "lfn": "/store/data/big.dat", "size": 10485760, "read_write": false },
  "transfer": { "operation": "read", "open_seen": true,
                "start_time": "2026-06-23T15:57:50Z",
                "end_time": "2026-06-23T15:57:53Z", "duration_s": 3,
                "forced_close": false, "is_local": false,
                "read_bytes": 10485760, "readv_bytes": 0, "write_bytes": 0,
                "read_ops": 2, "readv_ops": 0, "write_ops": 0 },
  "activity": { "experiment_id": 1, "activity_id": 7,
                "experiment": "cms", "activity": "production" },
  "app":    { "name": "xrdcp", "raw": "..." }
}
```

`transfer.open_seen` is `false` (and the `file`/`user`/`client` objects are
absent) for a close whose open record was lost or predates the collector — the
`transfer` byte totals are still reported. Empty/zero fields are omitted, so a
given document only carries what the server actually reported.

### WLCG field mapping

The schema covers the WLCG transfer-monitoring fields that XRootD currently puts
on the wire. Mapping (and the server config each needs):

| WLCG field | Document field | Source / requires |
| :-- | :-- | :-- |
| file_name | `file.lfn` | `fstat … lfn` |
| operation_type | `transfer.operation` (`read`/`write`) | `fstat … xfr` |
| operation_state | `transfer.operation_state` (`Successful`/`Failed`/`Redirected`) | `fstat` (terminal report); `Redirected` from `r` with `--redirects` |
| error_message | `transfer.error_message` | `fstat` (failed open / I/O / close) |
| error_category | `transfer.error_category` + `transfer.error_code` | `fstat` (failed open / I/O / close) |
| server_name/site | `server.name` / `server.site` | `=` ident (`XRDSITE` for site) |
| server_ip / hostname | `server.ip` / `server.hostname` | UDP source / `=` ident (loopback → local FQDN) |
| client_ip / hostname | `client.ip` / `client.hostname` | `u` descriptor (server DNS config) |
| client_version | `client.version` | login appinfo (`&R=`) |
| ip_version | `client.ip_version` | login appinfo (`&I=`) |
| client_site | `client.site` | login appinfo (`&S=`, client `XRDSITE`/`XRD_SITE`) |
| auth_method | `user.auth_method` | **`… auth`** |
| user | `user.name` / `user.subject` | `u` / `T` token |
| vo | `user.vo` | `T` token, else `… auth` (`&o=`), else SciTags experiment (`--scitags`) |
| activity | `activity.experiment`/`activity.activity` (names), `activity.*_id` (numeric), `user.role` | `U` SciTags + `--scitags` registry; `T` token for role |
| start_time / end_time | `transfer.start_time` / `.end_time` | f-stream `FileTOD` window |
| bytes | `transfer.{read,readv,write}_bytes` | `fstat … xfr` |
| is_local (LAN/WAN) | `transfer.is_local` | derived: client vs server domain (needs `=` ident) |

`server.hostname` precedence is: the host advertised on the `=` ident stream
(when it is a real name, not an IP literal), else — for a server reporting from
the loopback address (the common co-located collector + server setup, where the
UDP source is `::1`/`127.0.0.1`) — the collector's own local FQDN, since the
reporting server runs on the same host. `server.name` falls back to the numeric
`server.ip` when neither is available. `--no-resolve` disables the loopback
substitution (leaving the numeric address). A *remote* server is never
reverse-resolved here — that hostname comes from its `=` ident, and a blocking
reverse-DNS lookup of an arbitrary source IP would stall the UDP receive loop.

`transfer.is_local` is a heuristic: it is `true` when the client and the
reporting server share a registered domain (the part after the first host
label), `false` when they differ, and **omitted** when either side is an IP
literal or the server host is unknown (no `=` ident yet). It also drives
`xrootd_collector_locality_transfers_total{server,locality}`.

`transfer.operation_state` is the authoritative success/failure of the
operation: a plain close reports `Successful`, while a failed open, a
mid-transfer read/write error, or a failed close reports `Failed` together with
`transfer.error_category` (`open`/`read`/`write`/`close`/`auth`),
`transfer.error_code` (the XRootD error code), and `transfer.error_message`. A
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
(`transfer.forced_close`) close is **not** a failure unless an error was
actually recorded. The `error_message` is the server's own SFS reason verbatim
(e.g. `Unable to open …; no such file or directory` for a missing file); the
`XRootD::moncollect` test asserts that specific reason per-document for both a
failed open and the readv-past-EOF case.

A third terminal state, `"Redirected"`, is reported for `r`-stream redirect
records (with `--redirects`): from the redirector's point of view the operation
concluded by sending the client elsewhere. The redirect destination travels
under the `redirect` object (`kind`, `target_host`, `target_port`); the data
server that ultimately serves the file emits its own `Successful`/`Failed`
close.

`client.site` is the site the *client* advertises for itself: an XRootD client
that has `XRDSITE` (or `XRD_SITE`, which takes precedence) set in its environment
sends it in the login CGI (`xrd.site`), the server folds it into the user-map
appinfo (`&S=`), and the collector surfaces it as `client.site`. It is absent
when the client does not advertise one. (This is distinct from `server.site`,
which is the *reporting server's* `XRDSITE` from the `=` ident record.)

## Aggregated metrics (Prometheus)

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

## Notes and limitations

- Correlation state (the user/path/token/activity dictionaries and the open-file
  table) is kept per server incarnation, keyed by sender address plus the server
  start time. An open is freed by its close, but a close (or a session
  disconnect) can be lost to a dropped datagram, a client/server crash, or a
  restart — and the server never reuses a dictid within an incarnation — so the
  state would otherwise grow without bound. `--max-memory` (default 256M; 0 =
  unbounded) bounds it to an approximate byte budget, evicting the
  *least-recently-used* entries when exceeded. Recency is what protects a genuine
  long-running transfer: each in-flight `f`-stream (`xfr`) snapshot, and each
  reference of a session by a close, promotes the entry, so a file left open for
  a day survives as long as there is memory while cold, stranded entries are
  dropped first. `--max-entries` adds an optional hard entry-count backstop
  (off by default). Evictions are counted in `xrootd_collector_evicted_total`,
  and the live budget utilisation is the `xrootd_collector_state_bytes` gauge. A
  dropped entry merely yields a document missing that field, or an orphan close.
- Whole server incarnations are reclaimed once idle past `--server-ttl` (default
  24h; 0 = never), so dead incarnations from restarts and rolling upgrades do not
  accumulate. Reclaimed incarnations are counted in
  `xrootd_collector_reaped_servers_total`.
- UDP is lossy: a lost open record yields an orphan close; a lost dictionary
  record yields a document without identity/path. The server stamps every
  datagram to one destination with a single sequence number (header `pseq`), so
  the collector estimates loss from forward gaps in it —
  `xrootd_collector_packets_lost_total{server}` and the `-v` `lost=` count.
  (Reordering, a small backward step, is not counted as loss.)
- Only the `f` stream is correlated today. The `t` (per-I/O trace) and `g`
  (plugin) streams are decoded enough to be counted; turning them into
  documents/metrics is future work.

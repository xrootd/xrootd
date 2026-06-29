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
  --forward <h:p>  also stream documents as NDJSON over TCP to host:port
  --flush-count <n> flush after N documents (default: 500)
  --flush-secs <n>  flush after N seconds (default: 5)
  --metrics-port <p> serve aggregated metrics over HTTP on port <p>
  --max-entries <n>  cap per-server dict/open-file entries (0=unbounded)
  --traces         emit a document per t-stream I/O record (high volume)
  --gstream        emit a document per g-stream (plugin) record
  --redirects      emit a document per r-stream redirect record
  --dump           also emit one JSON object per decoded record (debugging)
  -v               print decoder statistics on exit (SIGINT/SIGTERM)
```

All document types — `transfer`, `session_end`, `server_ident`, `frm`,
`redirect`, the `t`-stream traces and `gstream` — share one nested,
OpenSearch-friendly schema (`server.*`, `client.*`, `user.*`, `file.*`,
`transfer.*`, …; each nested object indexes as a dotted field).

### Streams

By default the `f` (file-stats) stream produces a per-transfer document on each
file close, a `session_end` document on each client disconnect (`isDisc`
record, with the resolved user), and maintains the
`xrootd_collector_active_transfers{server}` gauge (open files in progress, from
the `isXfr` snapshots and open/close records). Two opt-in streams add
finer-grained events:

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
- `--redirects` turns each `r` (redirect) record into a document: operation,
  remote/local kind, the destination `target.host`/`target.port`, the redirected
  `file.lfn`, and the joined `user`/`client`. Emitted mainly by
  redirectors/managers; requires `redir` in the monitor `dest` list.

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

### Sinks

Documents fan out to any combination of sinks; stdout is used only as the
fallback when no other sink is configured (`-o` always adds a file too):

- **File / stdout** (`-o`, `--bulk`): NDJSON, or the OpenSearch `_bulk` framing
  with `--bulk <index>`. Ship it with an external agent (Filebeat) or `curl`.
- **OpenSearch** (`--os-url`): available when the binary is built with libcurl
  (the build links `CURL::libcurl` if found). Documents are batched and posted
  via the `_bulk` API; transient failures (network, HTTP 429/5xx) are retried
  with exponential backoff.
- **TCP forward** (`--forward host:port`): streams the same NDJSON over a plain
  TCP connection to a buffering/forwarding frontend — Logstash (`tcp` input),
  Fluentd (`in_tcp`), Vector (`socket` source), or a message-broker bridge. The
  connection is lazily (re)established with a short cool-down; documents
  produced while the consumer is down are dropped (durable buffering is the
  downstream's job). Dependency-free, so it is built even without libcurl.

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
              "version": "v5.6.1", "ip_version": 4 },
  "user":   { "name": "amadio", "protocol": "xroot", "auth_method": "gsi",
              "vo": "atlas", "role": "production", "subject": "https://issuer/sub42" },
  "file":   { "lfn": "/store/data/big.dat", "size": 10485760, "read_write": false },
  "transfer": { "operation": "read", "open_seen": true,
                "start_time": "2026-06-23T15:57:50Z",
                "end_time": "2026-06-23T15:57:53Z", "duration_s": 3,
                "forced_close": false, "is_local": false,
                "read_bytes": 10485760, "readv_bytes": 0, "write_bytes": 0,
                "read_ops": 2, "readv_ops": 0, "write_ops": 0 },
  "activity": { "experiment_id": 1, "activity_id": 7 },
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
| operation_state | `transfer.operation_state` (`Successful`/`Failed`) | `fstat` (terminal report) |
| error_message | `transfer.error_message` | `fstat` (failed open/close) |
| error_category | `transfer.error_category` + `transfer.error_code` | `fstat` (failed open/close) |
| server_name/site | `server.name` / `server.site` | `=` ident (`XRDSITE` for site) |
| server_ip / hostname | `server.ip` / `server.hostname` | UDP source / `=` ident |
| client_ip / hostname | `client.ip` / `client.hostname` | `u` descriptor (server DNS config) |
| client_version | `client.version` | login appinfo (`&R=`) |
| ip_version | `client.ip_version` | login appinfo (`&I=`) |
| auth_method | `user.auth_method` | **`… auth`** |
| user | `user.name` / `user.subject` | `u` / `T` token |
| vo | `user.vo` | `T` token, else `… auth` (`&o=`) |
| activity | `user.role`, `activity.*` | `T` token / `U` SciTags |
| start_time / end_time | `transfer.start_time` / `.end_time` | f-stream `FileTOD` window |
| bytes | `transfer.{read,readv,write}_bytes` | `fstat … xfr` |
| is_local (LAN/WAN) | `transfer.is_local` | derived: client vs server domain (needs `=` ident) |

`transfer.is_local` is a heuristic: it is `true` when the client and the
reporting server share a registered domain (the part after the first host
label), `false` when they differ, and **omitted** when either side is an IP
literal or the server host is unknown (no `=` ident yet). It also drives
`xrootd_collector_locality_transfers_total{server,locality}`.

`transfer.operation_state` is the authoritative success/failure of the
operation: a plain close reports `Successful`, while a failed open or a failed
close reports `Failed` together with `transfer.error_category`
(`open`/`read`/`write`/`close`/`auth`), `transfer.error_code` (the XRootD error
code), and `transfer.error_message`. A failed open never produced any close
record before; the server now emits a terminal `isError` f-stream record (and
sets `hasERR` on the close for a failed close), keyed on
`xrootd_collector_failed_operations_total{server,category}`. This requires only
the existing `fstat` setup — no extra directive. A disconnect-driven
(`transfer.forced_close`) close is **not** a failure unless a close error was
actually recorded.

**Still not on the wire**: a client-advertised `client.site` (only carried by
free-form appinfo by convention).

## Aggregated metrics (Prometheus)

With `--metrics-port <p>` the collector also runs a small HTTP exporter that
serves Prometheus metrics aggregated from the decoded transfers. Unlike the
per-transfer documents (which belong in a document store), these are bounded
in cardinality — labelled only by the reporting `server` — and suitable for a
time-series database:

```
xrootd_collector_transfers_total{server="..."}
xrootd_collector_read_bytes_total{server="..."}
xrootd_collector_write_bytes_total{server="..."}
xrootd_collector_vo_transfers_total{server="...",vo="..."}
xrootd_collector_locality_transfers_total{server="...",locality="local|remote"}
xrootd_collector_sessions_total{server="..."}
xrootd_collector_active_transfers{server="..."}   (gauge)
xrootd_collector_transfer_size_bytes        (histogram)
xrootd_collector_transfer_duration_seconds  (histogram)
xrootd_collector_packets_total              (and other decoder statistics)
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

- Correlation state (the user/path/token dictionaries and the open-file table)
  is kept per server incarnation, keyed by sender address plus the server start
  time. Each map is capped at `--max-entries` (default 1,000,000; 0 = unbounded)
  to bound memory on long-lived busy servers; eviction is approximate (hash
  order), so a dropped entry merely yields a document missing that field or an
  orphan close. The count is reported as `xrootd_collector_evicted_total`.
- UDP is lossy: a lost open record yields an orphan close; a lost dictionary
  record yields a document without identity/path. The server stamps every
  datagram to one destination with a single sequence number (header `pseq`), so
  the collector estimates loss from forward gaps in it —
  `xrootd_collector_packets_lost_total{server}` and the `-v` `lost=` count.
  (Reordering, a small backward step, is not counted as loss.)
- Only the `f` stream is correlated today. The `t` (per-I/O trace) and `g`
  (plugin) streams are decoded enough to be counted; turning them into
  documents/metrics is future work.

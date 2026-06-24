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
               [--os-pass <p>] [--os-insecure]]
              [--flush-count <n>] [--flush-secs <n>] [--dump] [-v]

  -p <port>        UDP port to listen on (required)
  -b <bindaddr>    address to bind (default: all interfaces, dual-stack)
  -o <file>        append output to <file> (default: stdout unless --os-url)
  --bulk <index>   write OpenSearch _bulk format to the file/stdout sink
  --os-url <url>   POST documents to an OpenSearch cluster's _bulk API
  --os-index <n>   index/data-stream name (default: xrootd-transfers)
  --os-user <u>    basic-auth user
  --os-pass <p>    basic-auth password
  --os-insecure    skip TLS certificate verification
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

### Streams

By default the `f` (file-stats) stream produces a per-transfer document on each
file close, a `session_end` document on each client disconnect (`isDisc`
record, with the resolved user), and maintains the
`xrootd_collector_active_transfers{server}` gauge (open files in progress, from
the `isXfr` snapshots and open/close records). Two opt-in streams add
finer-grained events:

- `--traces` turns each `t` (I/O trace) record into a document: `read`/`write`
  (with offset, length and the resolved `lfn`), `open`, `close`, `disconnect`,
  and `appid`. This is **high volume** (one record per I/O) — enable only when
  the detail is needed. Requires `io` in the server's monitor `dest` list and
  the path dictionary (`d` stream) to resolve file names.
- `--gstream` forwards each `g` (plugin) record — from the `oss`, `pfc`,
  `throttle`, `tpc`, `http` g-streams — as a document tagged with its provider,
  embedding the plugin's JSON payload. Requires `xrootd.mongstream` on the
  server. Independently of document emission, when `--metrics-port` is set the
  `oss`, `pfc` and `tpc` providers are also parsed into aggregate metrics (see
  below); the `oss` running totals are converted to counter deltas.
- `--redirects` turns each `r` (redirect) record into a document: operation,
  remote/local kind, target host/port, path, and the redirected user. Emitted
  mainly by redirectors/managers; requires `redir` in the monitor `dest` list.

The `u` (user), `d` (path) and `i` (appinfo) dictionaries are always consumed:
they resolve identities and paths for the other streams, and the appinfo (`i`)
is joined to each transfer document by session descriptor (adds an `appinfo`
field when the client set one).

The `=` (server identity), `T` (token) and `U` (user experiment/activity)
records are also always consumed:

- `=` (`MAPIDNT`) yields a one-off `server_ident` document per server
  incarnation (site, host, instance, program, version, port) and tags every
  transfer document with `site`/`server_inst`. Re-sent identically each
  `ident` interval; the collector emits the document only when it changes.
- `T` (`MAPTOKN`) carries the token identity (subject, VO, role, groups). Keyed
  by the user dictid, it joins onto each transfer as `token_subject`/`vo`/
  `role`/`groups`, and drives `xrootd_collector_vo_transfers_total{server,vo}`.
- `U` (`MAPUEAC`) carries the SciTags packet-marking flow labels (experiment
  and activity ids), joined onto transfers as `experiment_id`/`activity_id`.

The direct OpenSearch sink (`--os-url`) is available when the binary is built
with libcurl (the build links `CURL::libcurl` if found). Documents are batched
and posted via the `_bulk` API; transient failures (network, HTTP 429/5xx) are
retried with exponential backoff. Without libcurl, ship the file/`--bulk` output
with an external agent (Filebeat) or `curl`.

### Examples

```sh
# Collect to a file as NDJSON
xrdmoncollect -p 9930 -o /var/log/xrootd/transfers.ndjson -v

# Post directly to OpenSearch
xrdmoncollect -p 9930 --os-url https://opensearch:9200 \
              --os-index xrootd-transfers --os-user admin --os-pass secret

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
operation counts and sum-of-squares to the close record.

```
xrootd.monitor all flush 30 fstat 30 lfn ops ssq xfr 1 \
               dest fstat info user <collector-host>:9930
```

## Output document

One object per file close, for example:

```json
{
  "type": "transfer",
  "@timestamp": "2026-06-23T15:57:53Z",
  "server": "[::1]:42359", "server_id": 53605690318209, "server_start": 1782230264,
  "user": "amadio", "protocol": "xroot", "client_host": "localhost",
  "lfn": "/store/data/big.dat", "file_size": 10485760,
  "open_time": "2026-06-23T15:57:50Z", "close_time": "2026-06-23T15:57:53Z",
  "duration_s": 3,
  "read_bytes": 10485760, "readv_bytes": 0, "write_bytes": 0,
  "read_ops": 2, "readv_ops": 0, "write_ops": 0
}
```

`open_seen` is `false` (and path/user are absent) for a close whose open record
was lost or predates the collector — the byte totals are still reported.

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

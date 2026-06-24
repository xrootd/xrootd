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
xrdmoncollect -p <port> [-b <bindaddr>] [-o <file>] [--bulk <index>] [--dump] [-v]

  -p <port>       UDP port to listen on (required)
  -b <bindaddr>   address to bind (default: all interfaces, dual-stack IPv4+IPv6)
  -o <file>       append output to <file> (default: stdout)
  --bulk <index>  emit OpenSearch _bulk format for the given index
  --dump          also emit one JSON object per decoded record (debugging)
  -v              print decoder statistics on exit (SIGINT/SIGTERM)
```

### Example

```sh
# Collect to a file as NDJSON
xrdmoncollect -p 9930 -o /var/log/xrootd/transfers.ndjson -v

# Produce an OpenSearch bulk stream and ship it
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

## Notes and limitations

- Correlation state (the user/path dictionaries and the open-file table) is kept
  per server incarnation, keyed by sender address plus the server start time.
  It is in-memory and currently unbounded; long sessions on a busy server will
  grow it. TTL/LRU eviction is a planned refinement.
- UDP is lossy: a lost open record yields an orphan close; a lost dictionary
  record yields a document without identity/path. The `-v` statistics report
  these.
- Only the `f` stream is correlated today. The `t` (per-I/O trace) and `g`
  (plugin) streams are decoded enough to be counted; turning them into
  documents/metrics is future work.

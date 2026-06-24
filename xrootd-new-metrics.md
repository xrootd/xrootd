# XRootD Native Prometheus Metrics

## Motivation

XRootD currently has three independent monitoring channels:

- **Summary monitoring** (`xrd.report`) — periodic aggregate snapshots pushed
  over UDP in XML (or JSON for plugin counters).
- **Event monitoring** (`xrootd.monitor`) — real-time per-operation binary UDP
  streams (`u`/`d`/`t`/`f` packets).
- **G-Stream** (`xrootd.mongstream`) — plugin-specific structured event streams.

None of these is directly consumable by a Cloud-Native monitoring stack.
Prometheus, the de-facto standard for time-series metrics, expects to **scrape**
a `/metrics` HTTP endpoint that returns the text exposition format. Since XRootD
is already an HTTP server, it can expose such an endpoint natively.

This document describes a plan to add a first-class, type-aware metrics facility
to XRootD that is Prometheus-native, while keeping the existing XML/JSON summary
formats unchanged so current collectors keep working.

## Goals

1. A new **`XrdMetrics`** module providing Prometheus-style instruments
   (counters, gauges, histograms) with labels and `HELP`/`TYPE` metadata. This
   becomes the single source of truth for server counters.
2. A pull-based **`GET /metrics`** endpoint implemented as an
   `XrdHttpExtHandler` plugin, serving the text exposition format.
3. **Bridging** of existing plugin counters (registered today via `XrdMonRoll`)
   into the scrape output, so plugins such as the OSS stats, HTTP, and proxy
   cache appear in `/metrics` without modification.
4. **Migration** of the core server counters onto `XrdMetrics`, with the
   existing `xrd.report` XML/JSON output preserved byte-for-byte as a *view*
   over the same instruments.

Deferred to later work:
- UDP / Pushgateway-style push of the Prometheus format.
- An external collector that decodes the binary `u`/`d`/`t`/`f` event streams,
  correlates them, and emits per-transfer records — specified as
  **Phase 5** below.

## Design overview

```
  hot paths ──inc()──▶ XrdMetrics instruments (atomic-backed)
                              │  (single source of truth)
        ┌─────────────────────┼──────────────────────────┐
        ▼                     ▼                            ▼
  XrdMetricsRegistry    XrdXrootdStats::Stats()      XrdMonRoll bridge
   .Scrape(text)        (XML view, byte-compatible)   (plugin/addon sets)
        │                     │                            │
        ▼                     ▼                            ▼
  GET /metrics          xrd.report UDP (XML/JSON)    folded into Scrape()
```

The same instruments feed three sinks: the new Prometheus scrape, the legacy
`xrd.report` XML/JSON path (unchanged on the wire), and the `/metrics` HTTP
handler. Metric values are read from lock-free atomics at scrape time
(compute-on-scrape; no background aggregation job).

### Compatibility

- New `XrdMetrics` headers are additive and do not break the installed API.
- The `/metrics` endpoint is a separate plugin shared library; it uses only the
  existing `XrdHttpExtHandler` interface.
- The core-stats migration keeps the `xrd.report` XML/JSON output identical, so
  existing monitoring (e.g. `mpxstats`) is unaffected.

## Implementation phases

### Phase 1 — `XrdMetrics` module

A new `src/XrdMetrics/` component compiled into `XrdUtils`:

- `XrdMetricsCounter` — monotonic, `inc(v=1)`.
- `XrdMetricsGauge` — `set`/`inc`/`dec`.
- `XrdMetricsHistogram` — fixed bucket bounds, `observe(v)`.
- `XrdMetricsRegistry` — registers metric families (name + help + type), each
  with label-keyed series, and produces the Prometheus text exposition format
  via `Scrape()`. A process-wide registry is published in the XRootD
  environment under the key `XrdMetricsRegistry*` (mirroring the existing
  `XrdMonRoll*` mechanism) so plugins can register their own metrics.

### Phase 2 — `/metrics` endpoint

A new `libXrdHttpPrometheus.so` plugin implementing `XrdHttpExtHandler`:
matches `GET /metrics` and returns `registry.Scrape()` with
`Content-Type: text/plain; version=0.0.4`. Loaded via the existing directive:

```
http.exthandler prometheus libXrdHttpPrometheus.so [configfile]
```

### Phase 3 — Bridge existing plugin counters

An adapter walks the `XrdMonRoll` registry and emits each registered counter
set as Prometheus series, so existing plugin counters appear in `/metrics`
without any plugin changes.

### Phase 4 — Core-stats migration

The core server counters (xrootd protocol operations, links, scheduler,
process, buffers) are backed by `XrdMetrics` instruments, and the existing XML
emitters are rewritten as views reading those instruments — keeping
`xrd.report` output byte-identical, guarded by regression tests.

## Metric naming

Metrics follow Prometheus conventions: `xrootd_` prefix, `snake_case`, `_total`
suffix for counters, and base-unit suffixes (`_bytes`, `_seconds`). For
example:

```
# HELP xrootd_ops_read_bytes_total Total bytes read from disk
# TYPE xrootd_ops_read_bytes_total counter
xrootd_ops_read_bytes_total{instance="srv1"} 12345
```

---

# Phase 5 — Detailed Event-Stream Collector (next phase)

Phases 1–4 cover **summary** monitoring: aggregate counters, scraped from the
server. This phase tackles the **detailed event streams** produced by
`xrootd.monitor` (and the plugin `g`-streams from `xrootd.mongstream`). These
describe individual logins, file opens/closes, and I/O operations — one record
per event, pushed as binary UDP packets. They are the data needed to answer
"who read which file, how much, for how long," which aggregate metrics cannot.

The work is a standalone C++ program (working name **`xrdmoncollect`**) that
listens on a UDP socket, decodes and correlates the packets, and writes the
result to one or more sinks.

## Why a separate binary, and why two output targets

The event streams are **event-shaped and high-cardinality** (per user, per file,
per transfer). That distinction drives the whole design:

- **Prometheus is the wrong store for the raw events.** It is built for
  bounded-cardinality time series; per-file / per-user labels would explode the
  cardinality. Prometheus should receive only **aggregates** the collector
  computes (e.g. total bytes read/written, transfer-size and duration
  histograms, active-transfer gauges), broken down by *bounded* dimensions
  only (server, vo/project, read-vs-write, maybe access protocol).
- **OpenSearch (or any document/log store) is the right store for the detail.**
  One JSON document per completed transfer — full identity, path, byte counts,
  op counts, duration — is exactly its sweet spot, and high cardinality is fine.

So the collector is a **decode → correlate → fan-out** pipeline with pluggable
sinks: a document sink (OpenSearch bulk API, or NDJSON to file/stdout) and a
metrics sink (its own `/metrics` endpoint and/or remote-write). The server is
left untouched; the collector can run anywhere the UDP packets are routed, and
multiple servers can report to one collector.

> **Prior art.** The OSG/WLCG community runs collectors for this data (e.g. the
> Go `xrootd-monitoring-shoveler` and GLED). Writing a native C++ collector in
> this tree lets us **reuse the on-the-wire struct definitions directly** from
> `XrdXrootdMonData.hh`, so the decoder cannot drift from the producer, and lets
> us share `XrdNet` (sockets) and the project's bundled `nlohmann_json`.

## Input: the monitoring wire format

All definitions live in `src/XrdXrootd/XrdXrootdMonData.hh`; **all multi-byte
fields are network byte order** and variable-length records must be walked by
their length field, never `sizeof`. Every packet starts with an 8-byte
`XrdXrootdMonHeader { code, pseq, plen, stod }`:

- `code` — stream/record type (see below).
- `pseq` — per-stream packet sequence number (wraps at 256) for loss detection.
- `plen` — total packet length.
- `stod` — server start time; `(stod, server-id)` identifies one server
  *incarnation* and must key all collector state (a server restart resets dicts).

Packet/record types we consume:

| code | name | content |
|------|------|---------|
| `u`  | `MAPUSER` | user-identity dictionary: dictid → `prot/user.pid:sfd@host` (+auth) |
| `d`  | `MAPPATH` | path dictionary: dictid → LFN |
| `i`  | `MAPINFO` | appinfo dictionary |
| `=`  | `MAPIDNT` | server identification |
| `f`  | `MAPFSTA` | **file-stats stream**: per-open/close/xfr records with byte & op counts |
| `t`  | `MAPTRCE` | trace stream: individual read/write/readv/open/close events |
| `r`  | `MAPREDR` | redirect stream |
| `g`  | `MAPGSTA` | plugin `g`-stream payloads (oss/pfc/throttle/tpc/http), already CGI/JSON |

### Start with the `f` stream, not the `t` stream

The **`f` (fstat) stream already does per-file aggregation in the server**: each
file close emits a `XrdXrootdMonFileCLS` record carrying transfer byte totals
(`XrdXrootdMonStatXFR`), and optionally op counts (`XrdXrootdMonStatOPS`) and
sum-of-squares (`XrdXrootdMonStatSSQ`) when `fstat … ops ssq` is configured. So
a per-transfer document — the headline OpenSearch use case — is essentially
*one `f`-stream close record joined to two dictionaries*. The `t` stream, by
contrast, requires reassembling many individual I/O events into a session and is
only needed for fine-grained offset/latency analysis. **Implement `f` first.**

The join keys for a close event:
- `XrdXrootdMonFileCLS.Hdr.fileID` → the `d`/`MAPPATH` dictid → LFN.
- the matching open record's `XrdXrootdMonFileLFN.user` → the `u`/`MAPUSER`
  dictid → identity. (Requires retaining open records until the close arrives.)

## Correlation model and state

The collector is necessarily **stateful**:

1. **Dictionary tables** — per `(stod, srcServer)`, maps `dictid → identity` and
   `dictid → path`. Populated by `u`/`d`/`i` packets, referenced by `f`/`t`
   records. Needs TTL/LRU eviction (dictids are recycled) and a cap.
2. **Open-file table** — `f`-stream open records held until their close, to
   recover the LFN/user and compute wall-clock duration (`close.tEnd −
   open.tBeg`, bracketed by the `XrdXrootdMonFileTOD` time records).
3. **Loss / reordering tolerance** — UDP is lossy; use `pseq` gaps to count loss
   per stream, tolerate out-of-order arrival, and emit a partial document (or
   drop with a counter) when a referenced dictid or open record is missing.

This state is bounded and evictable; it is *not* a database. Long-term storage
is the sink's job.

## Architecture

```
        UDP :9930
            │  recvfrom (XrdNet)
            ▼
   ┌──────────────────┐   decode (XrdXrootdMonData.hh structs, ntoh*)
   │   packet decoder │──────────────┐
   └──────────────────┘              ▼
            │ u/d/i            ┌─────────────┐  f close (+ open join)
            ▼                  │ correlator  │───────────────┐
   ┌──────────────────┐        │  (stateful) │               ▼
   │ dictionary tables│◀──────▶│             │      ┌──────────────────┐
   └──────────────────┘        └─────────────┘      │  transfer event  │
                                      │              │   (JSON doc)     │
                                      │ aggregate    └──────────────────┘
                                      ▼                       │
                              ┌──────────────┐    ┌───────────┴─────────┐
                              │ metrics sink │    │  document sink      │
                              │ (/metrics)   │    │ OpenSearch bulk /   │
                              └──────────────┘    │ NDJSON file/stdout  │
                                                  └─────────────────────┘
```

The metrics sink can reuse the **`XrdMetrics`** registry and the Prometheus text
serializer built in Phases 1–4, so the collector exposes its own `/metrics`
with the same code path.

## Output schemas (illustrative)

**Per-transfer document (OpenSearch / NDJSON)** — one per file close:

```json
{
  "@timestamp": "2026-06-23T16:50:32Z",
  "server": "srv1.example.org:1094", "server_inc": 1750000000,
  "user": "alice", "protocol": "xroot", "client_host": "wn42.example.org",
  "vo": "atlas", "appinfo": "...",
  "lfn": "/store/data/file.root",
  "open": "2026-06-23T16:49:10Z", "close": "2026-06-23T16:50:32Z",
  "duration_s": 82.0,
  "read_bytes": 10485760, "readv_bytes": 0, "write_bytes": 0,
  "read_ops": 320, "readv_ops": 0, "write_ops": 0,
  "read_min": 4096, "read_max": 1048576
}
```

**Aggregated metrics (Prometheus)** — bounded labels only:

```
xrootd_transfer_bytes_total{server="srv1",op="read"}  ...
xrootd_transfers_total{server="srv1",op="read"}       ...
xrootd_transfer_size_bytes_bucket{op="read",le="..."} ...
xrootd_active_transfers{server="srv1"}                ...
xrootd_monitor_packets_total{stream="f"}              ...
xrootd_monitor_packets_lost_total{stream="f"}         ...
```

## Incremental implementation plan

1. **Raw decoder / `--dump`.** UDP listener that decodes every packet and prints
   it as one JSON object per record to stdout — no correlation. Immediately
   useful for debugging a live `xrootd.monitor` feed and validating struct
   parsing. Establishes the byte-order/length-walking helpers.
2. **Dictionary resolution.** Maintain the `u`/`d`/`i` tables; resolve dictids in
   the dumped `f`/`t` records to real identities and paths.
3. **`f`-stream correlation → transfer documents.** Join open↔close, compute
   durations, emit the per-transfer JSON. Add the NDJSON file/stdout sink.
4. **Document sink: OpenSearch.** Batch documents to the `_bulk` API (with
   backpressure, retry, and a configurable index/data-stream name).
5. **Metrics sink.** Aggregate transfers into `XrdMetrics` instruments; expose a
   `/metrics` endpoint (reuse the Phase 1 serializer) and/or remote-write.
6. **`t`-stream and `g`-stream.** Add per-I/O trace correlation for fine-grained
   analysis, and forward/translate the plugin `g`-stream JSON payloads.

Steps 1–3 deliver the core value (searchable per-transfer records); 4–6 are
independent add-ons.

> **Status: steps 1–4 and 6 implemented** as the `xrdmoncollect` binary under
> `src/XrdApps/XrdMonCollect/` (decoder/correlator in `XrdMonDecode`, UDP loop
> and sinks in `XrdMonCollect.cc`, OpenSearch `_bulk` HTTP sink in
> `XrdMonOpenSearch.cc`), with unit tests in `tests/XrdMonCollectTests/` and a
> `README.md`. Verified end-to-end against a live server and a mock OpenSearch
> endpoint:
> - **f-stream** → per-transfer documents (lfn, user, byte/op counts, duration);
> - **OpenSearch sink** (`--os-url`, libcurl, batched `_bulk` with retry);
> - **t-stream** (`--traces`) → read/write/open/close/disconnect documents with
>   file names resolved from the `d` path dictionary;
> - **g-stream** (`--gstream`) → plugin (oss/pfc/throttle/tpc/http) records
>   forwarded with their JSON payload.
>
> Step 5 (Prometheus aggregate sink) is also implemented (`--metrics-port`),
> as are the `i` (appinfo, joined to transfers) and `r` (redirect, `--redirects`)
> streams. The remaining streams are mapped out in **Phase 6** below.
>
> **Important server-config finding:** the `f`-stream emits the per-file
> `isClose` record (hence a transfer document) only when the **`xfr`** option is
> present in the `fstat` directive — without it the server registers opens but
> never reports closes (`XrdXrootdMonFile::Open` assigns the monitor entry only
> when I/O stats are kept). The collector is also dual-stack: it binds IPv4+IPv6
> so it receives packets when the server resolves the destination to `::1`.

## Build, placement, and configuration

- **Placement.** A new app under `src/XrdApps/` (alongside `XrdMpxStats.cc`),
  producing the `xrdmoncollect` binary. Reuse `XrdNet` for the socket,
  `XrdSys` utilities, the in-tree monitoring structs, and bundled
  `nlohmann_json`; link `XrdUtils` (which now includes `XrdMetrics`).
- **Configuration.** Simple CLI/flags (or a small config file): listen
  `host:port`, sink selection and endpoints, dictionary cache size/TTL, output
  index name, log level. Point a server at it with:

  ```
  xrootd.monitor all flush 30s ident 5m fstat 60s lfn ops ssq xfr 1 \
                 dest fstat info user io redir <collector-host>:9930
  ```
  (`xfr` is required for close records — see the status note above.)

## Testing

- **Unit tests** decode fixed byte buffers (captured or hand-built) for each
  record type and assert the parsed fields — guards against wire-format drift.
- **Replay** captured `.pcap`/raw UDP dumps through the collector for regression
  testing of correlation and output.
- **Integration**: run a real `xrootd` with `xrootd.monitor … dest … :9930`,
  drive `xrdcp` traffic, and assert that a transfer document and the expected
  metric deltas appear.

## Open questions / decisions for this phase

- **Primary sink priority** — OpenSearch document store first (richest payoff),
  with the Prometheus aggregate sink as a follow-on? (Recommended.)
- **OpenSearch shape** — plain index vs. data stream; index naming/rotation;
  whether to ship via the collector directly or via an intermediate
  (e.g. message queue) for buffering at scale.
- **Identity enrichment** — how much to derive (VO/project, DN parsing) in the
  collector vs. downstream ingest pipelines.
- **Deployment** — one collector per site vs. per host; HA/duplication strategy
  given UDP's at-most-once delivery.

---

# Phase 6 — Remaining monitoring streams & activity types (next session)

`xrdmoncollect` currently decodes `u` (user), `d` (path), `i` (appinfo, joined
to transfers), `f` (file stats → transfer docs + metrics), `t` (I/O traces),
`g` (plugin streams, forwarded as opaque JSON), `r` (redirects), and — as of
Phase 6 item 1 — `=` (server identity), `T` (token), and `U` (user experiment/
activity). This phase maps out what is left and how to add it.

> **Item 1 DONE.** `=` (`MAPIDNT`) → a `server_ident` document (site/host/
> instance/program/version/port) plus `site`/`server_inst` enrichment on every
> transfer; `T` (`MAPTOKN`) → `token_subject`/`vo`/`role`/`groups` joined by user
> dictid, plus `xrootd_collector_vo_transfers_total{server,vo}`; `U` (`MAPUEAC`)
> → SciTags `experiment_id`/`activity_id`. All keyed by the user dictid (same as
> `u`), reusing `DecodeMap`. Unit-tested (`TokenAndActivityEnrichTransfer`,
> `ServerIdentDecoded`). Remaining items below are unchanged.

## A. Top-level packet codes still unhandled

All are defined in `src/XrdXrootd/XrdXrootdMonData.hh`; map-type records reuse
the `XrdXrootdMonMap` layout (`header + dictid + info`) and are trivial to add
alongside the existing `DecodeMap`.

| code | name | content / route | value | effort |
|------|------|-----------------|-------|--------|
| `=` | `MAPIDNT` | server self-identification: site, host, port, instance, pgm. Not a `dictid` map — a one-off identity string. | server metadata enrichment | **DONE** |
| `T` | `MAPTOKN` | token dictionary (`dictid → token info`: subject, VO, role, groups from SciTokens/JWT). Routed as USER (`Map()`), so it is an `XrdXrootdMonMap`. | identity enrichment — attach token subject/VO to transfers | **DONE** |
| `U` | `MAPUEAC` | user experiment/activity: SciTags packet-marking experiment (`Ec`) and activity (`Ac`) flow labels. Also a USER-routed map. | VO/activity enrichment of transfers | **DONE** |
| `x` | `MAPXFER` | FRM transfer/migration record (stage-in/out). Format is FRM-specific — needs investigation in `XrdFrm`/`XrdXrootdMonFile` xfr path. | stage/migration visibility | medium |
| `p` | `MAPPURG` | FRM/cache purge record. FRM-specific format. | purge/eviction visibility | medium |
| `m` | `MAPMIGR` | **internal use only** — skip. | — | — |
| `s` | `MAPSTAG` | **internal use only** — skip. | — | — |

`T` and `U` are the high-value quick wins: they are `XrdXrootdMonMap`s keyed by
the same session descriptor as `u`/`i`, so they slot straight into `DecodeMap`
and the descriptor-join already used for appinfo (store `dictid → {subject, vo,
…}` and enrich `EmitClose`).

## B. g-stream provider schemas (currently opaque)

The `g` stream is forwarded verbatim (provider tag + JSON payload) — already
useful for OpenSearch. The work here is **per-provider structured parsing and
aggregation into bounded Prometheus metrics**. Provider type is the top byte of
`sID` (see `gsProvider()`); each has its own record schema:

> **Item 2 DONE for oss/pfc/tpc.** `gsAggregate()` parses these three providers
> into bounded metrics (independently of `--gstream` doc emission): `oss`
> running-total op counts → counter deltas (`xrootd_collector_oss_ops_total`,
> `..._slow_ops_total`, label `op`); `pfc` per-`file_close` byte counts
> (`xrootd_collector_pfc_files_total`, `..._bytes_total{source}`); `tpc`
> per-copy (`xrootd_collector_tpc_total{type,result}`, `..._bytes_total{type}`,
> `..._size_bytes` histogram). The delta tracker (`gsPrev`) skips the first
> snapshot as a baseline and treats a decrease as a counter reset. Unit-tested
> (`GStreamOssMetricsDelta`, `GStreamPfcAndTpcMetrics`). throttle/tcpmon/ccm/
> http remain forwarded-only (see below).

| provider | byte | content | candidate metrics |
|----------|------|---------|-------------------|
| `oss` | `O` | OSS plugin op counts/timings (read/write/stat/open + "slow" variants) — already Prometheus-shaped (see `XrdOssStats`) | **DONE**: per-op + slow-op counters (timings still TODO) |
| `pfc` | `C` | proxy file cache: bytes from cache/disk/origin, hits/misses, prefetch | **DONE**: file closes + bytes by source |
| `ccm` | `M` | cache context mgmt: file admit/purge decisions | admit/purge rates, residency |
| `tpc` | `P` | third-party copy: push/pull, src/dst, bytes, status, duration | **DONE**: copies by type/result, bytes, size histogram |
| `throttle` | `R` | throttle plugin: I/O rates, wait times, limited ops | throttled-bytes, wait-seconds |
| `tcpmon` | `T` | TCP connection stats: RTT, retransmits, bytes, congestion window | per-conn RTT/retransmit histograms |
| `http` | `H` | HTTP request processing activity | request rate by method/status (overlaps the http_plugin summary metrics) |

Recommended order by payoff: **oss, pfc, tpc** first (storage/cache/copy
throughput), then throttle/tcpmon/ccm/http. Each provider's records would feed
(a) a structured document and (b) aggregate counters/histograms.

## C. Activity types already received but not surfaced

- **`f`-stream `isXfr`** (in-flight transfer snapshots, enabled by `xfr <n>`):
  **DONE** (counted; drives the `xrootd_collector_active_transfers{server}`
  gauge derived from the open-file table). Per-file live-throughput deltas are
  still TODO (would need to avoid double-counting the close totals).
- **`f`-stream `isDisc`** (session end): **DONE** — emits a `session_end`
  document with the resolved user and increments
  `xrootd_collector_sessions_total{server}`. A login→disconnect byte total
  would require tracking per-session bytes (TODO).
- **`t`-stream `REDHOST` (0xf0)**: skipped; a per-client redirect marker that
  could complement the `r` stream.
- **`t`-stream `readu`**: decoded as `readv`; could be split out.

## D. Cross-cutting collector work (still TODO)

- **Dictionary/open-file eviction.** **DONE** — each per-server map
  (`users`/`paths`/`infos`/`tokens`/`activity`/`files`) is capped at
  `--max-entries` (default 1M), evicting in hash order back to ~90% when
  exceeded; count reported as `xrootd_collector_evicted_total`. TTL-based
  eviction (vs. the current size cap) is a possible refinement.
- **Loss detection.** **DONE** — the server stamps every datagram to one
  destination with a single header `pseq` (not per stream: it is a shared
  `seq1++` in `XrdXrootdMonitor::Send`), so loss is estimated per `(stod, src)`
  from forward gaps → `xrootd_collector_packets_lost_total{server}` (small
  backward steps are treated as reordering, not loss).
- **VO/identity enrichment** from `T`/`U`/DN parsing, as a bounded metric label.

## E. Suggested implementation order (next session)

1. ~~`=` MAPIDNT server identity + `T`/`U` token/VO maps~~ **(DONE)**.
2. ~~g-stream structured parsing + metrics for `oss`, `pfc`, `tpc`~~ **(DONE)**.
3. ~~`f`-stream `isXfr`/`isDisc` → active-transfer gauges and session docs~~ **(DONE)**.
4. ~~`pseq` loss detection and dictionary eviction (robustness)~~ **(DONE)**.
5. Remaining g-stream providers (throttle/tcpmon/ccm/http) and `x`/`p` (FRM).

Each step is independently testable with hand-built packets (the established
`tests/XrdMonCollectTests/` pattern) and, where a live source exists, against a
configured server (a redirector for `r`, a proxy cache for `pfc`, a TPC for
`tpc`, etc.).

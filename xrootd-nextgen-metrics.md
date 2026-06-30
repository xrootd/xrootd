# XRootD Next-Generation Cloud Native Monitoring System

## Motivation

XRootD has three independent monitoring channels:

- **Summary monitoring** (`xrd.report`) — periodic aggregate snapshots pushed
  over UDP in XML (or JSON for plugin counters).
- **Detailed monitoring** (`xrootd.monitor`) — real-time per-operation binary UDP
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
   (counters, gauges, histograms, and summaries) with labels and `HELP`/`TYPE` metadata.
   This becomes the single source of truth for server counters.
2. A pull-based **`GET /metrics`** endpoint implemented as an
   `XrdHttpExtHandler` plugin, serving the text exposition format.
3. **Bridging** of existing plugin counters (registered today via `XrdMonRoll`)
   into the scrape output, so plugins such as the OSS stats, HTTP, and proxy
   cache appear in `/metrics` without modification.
4. **Migration** of the core server counters onto `XrdMetrics`, with the
   existing `xrd.report` XML/JSON output preserved byte-for-byte as a *view*
   over the same instruments.

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

A new `libXrdHttpMetricsExporter.so` plugin implementing `XrdHttpExtHandler`:
matches `GET /metrics` and returns the Prometheus text exposition format with
`Content-Type: text/plain; version=0.0.4`. Loaded via the existing directive:

```
http.exthandler metrics libXrdHttpMetricsExporter.so [/path]
```

Beyond the pull endpoint, the plugin can also **push** the same registry to
remote collectors in the background (libcurl): a Prometheus Pushgateway (PUT,
text format) and/or an OTLP/HTTP receiver (POST, OpenTelemetry OTLP/JSON, via
`OtelJsonSerializer`). See `src/XrdHttpMetricsExporter/README.md` for the
`metrics.*` directives.

### Phase 3 — Bridge existing plugin counters

An adapter walks the `XrdMonRoll` registry and emits each registered counter
set as Prometheus series, so existing plugin counters appear in `/metrics`
without any plugin changes.

### Phase 4 — Core-stats migration

The core server counters (xrootd protocol operations, links, scheduler,
process, buffers) are backed by `XrdMetrics` instruments, and the existing XML
emitters are rewritten as views reading those instruments — keeping
`xrd.report` output byte-identical, guarded by regression tests.

#### Metric naming

Metrics follow Prometheus conventions: `xrootd_` prefix, `snake_case`, `_total`
suffix for counters, and base-unit suffixes (`_bytes`, `_seconds`). For
example:

```
# HELP xrootd_ops_read_bytes_total Total bytes read from disk
# TYPE xrootd_ops_read_bytes_total counter
xrootd_ops_read_bytes_total{instance="srv1"} 12345
```

---

### Phase 5 — xrdmoncollect — Detailed Event-Stream Collector (next phase)

- An external collector that decodes the binary `u`/`d`/`t`/`f` event streams,
  correlates them, and emits per-transfer records — specified as
  **Phase 5** below.

Phases 1–4 covered **summary** monitoring: aggregate counters, scraped from the
server. This phase tackles the **detailed event streams** produced by
`xrootd.monitor` (and the plugin `g`-streams from `xrootd.mongstream`). These
describe individual logins, file opens/closes, and I/O operations — one record
per event, pushed as binary UDP packets. They are the data needed to answer
"who read which file, how much, for how long," which aggregate metrics cannot.

The work is a standalone C++ program (working name **`xrdmoncollect`**) that
listens on a UDP socket, decodes and correlates the packets, and writes the
result to one or more sinks.

#### Why a separate binary, and why two output targets

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

#### Input: the monitoring wire format

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

#### Start with the `f` stream, not the `t` stream

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

#### Correlation model and state

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

#### Architecture

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

#### Output schemas (illustrative)

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

#### Incremental implementation plan

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

#### Build, placement, and configuration

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

#### Testing

- **Unit tests** decode fixed byte buffers (captured or hand-built) for each
  record type and assert the parsed fields — guards against wire-format drift.
- **Replay** captured `.pcap`/raw UDP dumps through the collector for regression
  testing of correlation and output.
- **Integration**: run a real `xrootd` with `xrootd.monitor … dest … :9930`,
  drive `xrdcp` traffic, and assert that a transfer document and the expected
  metric deltas appear.

#### Open questions / decisions for this phase

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

### Phase 6 — Remaining monitoring streams & activity types (next session)

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
| `x` | `MAPXFER` | FRM stage/migrate record — an `XrdXrootdMonMap` (dictid 0) with info `"<who>\n<path>"` (the wire `x` code covers both stage and migrate). | stage/migration visibility | **DONE** |
| `p` | `MAPPURG` | FRM purge record — same map layout with a `"\n&tod=&sz=&at=&ct=&mt=&fn="` CGI tail carrying the purged file size. | purge/eviction visibility | **DONE** |
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
| `ccm` | `M` | cache context mgmt: file admit/purge decisions | admit/purge rates, residency (forwarded-only) |
| `tpc` | `P` | third-party copy: push/pull, src/dst, bytes, status, duration | **DONE**: copies by type/result, bytes, size histogram |
| `throttle` | `R` | throttle plugin: I/O rates, wait times, limited ops | **DONE**: io_total counter + io_active gauge (io_wait TODO) |
| `tcpmon` | `T` | TCP connection stats: RTT, retransmits, bytes, congestion window | per-conn RTT/retransmit histograms (forwarded-only; plugin-defined schema) |
| `http` | `H` | HTTP request processing activity | **DONE**: requests by method/status (delta of cumulative counts) |

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
5. ~~Remaining g-stream providers (throttle/http) and `x`/`p` (FRM)~~ **(DONE)**.
   `ccm` and `tcpmon` remain forwarded-only (no fixed in-tree JSON schema:
   `tcpmon` records are produced by a `XrdTcpMonPin` plugin, `ccm` by the pfc
   cache-context manager); structured parsing waits on a concrete schema.

Each step is independently testable with hand-built packets (the established
`tests/XrdMonCollectTests/` pattern) and, where a live source exists, against a
configured server (a redirector for `r`, a proxy cache for `pfc`, a TPC for
`tpc`, etc.).

---

### Phase 7 — WLCG transfer-report alignment

`xrdmoncollect` is intended to become the sole monitoring solution for the
detailed event streams, replacing the Go shoveler/GLED for WLCG/CMS. Their
required per-operation report is specified in `wlcg-xrootd-collector-requirements.md`
(modeled on the FTS events in `fts-examples.md`). The gap analysis split cleanly
into two tiers.

#### Tier 1 — collector enrichment (DONE)

Everything WLCG asks for that XRootD already puts on the wire is now emitted in
the per-close transfer document, redesigned into an **OpenSearch-friendly nested
schema** (each nested object indexes as a dotted field: `server.name`,
`client.ip`, `transfer.read_bytes`). The key fix: the `u` (MAPUSER) record's
CGI tail — previously discarded — is now parsed in `DecodeMap`, recovering
`auth_method` (`&p=`), `ip_version` (`&I=`), `client_version` (`&R=`),
`app.name`/`info` (`&x=`/`&y=`), and a VO/role/group fallback (`&o=`/`&r=`/`&g=`,
used when the `T` token stream is absent). Also added: server host/IP/site joined
into the transfer doc, explicit `transfer.operation` (read/write), and
client IP-vs-hostname classification. `auth_method` and auth-derived VO require
`xrootd.monitor … auth`; the rest needs only the existing `fstat … lfn ops ssq
xfr … user info` setup. See `src/XrdApps/XrdMonCollect/README.md` for the full
field-to-WLCG mapping. Unit-tested by `Transfer.AuthTailEnrichesTransfer`,
`Transfer.NoAuthLoginAppinfoStillEnriches`, and `Transfer.WriteOperationDerived`.

#### Tier 2 — server-side terminal report (DONE)

Three WLCG fields were **emitted by nothing**: `operation_state` (authoritative
success/failure), `error_message`, and `error_category`. A *failed* open/transfer
produced no `f`-stream close record at all — only successful closes were reported.
The server now emits a terminal report on the `f`-stream so the collector can
fill these fields.

**Approach chosen: extend the f-stream** (not a dedicated new stream). This was
gated on *not* breaking unmodified pre-existing collectors, and verified against
the OSG sources: the **shoveler** (`verify.go`) forwards packets without
inspecting the code byte or record contents; the **OSG Go collector**
(`parser/xrootd_parser.go`) walks f-stream records by `recSize` with a
`default: seek(pos+RecSize)` for unknown record types (already exercised by
`isXfr`); and the XRootD monitoring spec documents recSize-walking as *the*
additive-extension mechanism. A brand-new top-level packet code, by contrast,
hits the OSG collector's `default: return error "unknown packet type"` — so it
would be dropped by unmodified collectors. `XrdXrootdMonData.hh` is a wire-format
header (not an installed public API header), so this is additive, not an ABI
break.

What landed (see git history `[XrdXrootd]`/`[XrdMonCollect]`):

- **Wire** (`XrdXrootdMonData.hh`): a new `isError` (recType 5) record and a
  `hasERR` (0x08) close flag, plus a variable-length `XrdXrootdMonStatERR`
  {error code, `monErrCat` category, null-terminated message} and the
  self-contained `XrdXrootdMonFileERR` (carries the lfn + user dictid inline,
  since a failed open creates no path/open dictionary entry).
- **Producer**: `XrdXrootdMonFile::OpenErr` emits the `isError` record from the
  terminal-error branch of `fsError` for open opcodes (failed/denied opens);
  `XrdXrootdFileStats::setCloseErr` + `XrdXrootdMonFile::Close` append the error
  block with `hasERR` on a genuine close failure (e.g. a failed upload commit),
  captured in `do_Close`. Disconnect-driven (`forced`) closes are not failures
  unless a close error was recorded.
- **Collector**: `EmitError` (isError) and the `hasERR` branch of `EmitClose`
  set `transfer.operation_state` (`Successful`/`Failed`),
  `transfer.error_category`/`error_code`/`error_message`, and a
  `xrootd_collector_failed_operations_total{server,category}` counter. Unit-
  tested (`Transfer.FailedOpenEmitsFailedState`, `AbortedTransferCloseHasError`,
  `SuccessfulCloseStateIsSuccessful`) and exercised end-to-end by the
  `XRootD::moncollect` integration test (`tests/XRootD/moncollect.{cfg,sh}`).

**Mid-transfer read/write errors (DONE, follow-on to Tier 2).** Terminal
failures of `read`/`readv`/`pgread`/`write`/`writev`/`pgwrite` are now captured
too, not just failed opens and failed closes. `XrdXrootdProtocol::monIOErr`
records the SFS error (code/category/message) on the file's `FileStats` at each
point the I/O paths invoke `fsError` (in `XrdXrootdXeq.cc` `do_ReadAll`,
`do_ReadV`, `do_WriteNoneMsg`, `do_WriteVec` and `XrdXrootdXeqPgrw.cc`
`do_PgRIO`; pgwrite converges on `do_WriteNoneMsg`). The existing
`setCloseErr` + `hasERR` close machinery then reports it — no wire-format or
collector change was needed. Semantics: last error wins, and a subsequent
failed close (`monErrClose`) overrides an earlier I/O error. The common
"readv past EOF" case is reported as a `read` failure with that message, which
is exactly the kind of reason WLCG consumers want. Exercised end-to-end by the
`xrdreadv-eof` driver in the `XRootD::moncollect` test
(`error_category:"read"`, e.g. `Unable to readv …; illegal seek`).

**Error-reason coverage (test hardening).** The `XRootD::moncollect` test now
asserts the *specific* terminal reason on a single document for both an open
failure (`xrdcp` of a missing file → `error_category:"open"`,
`error_code:3011` (`kXR_NotFound`), `error_message:"Unable to open …; no such
file or directory"`) and the mid-transfer readv failure (`error_category:"read"`,
`error_code:3005` (`kXR_FSError`), `…illegal seek`), rather than merely checking
for a non-empty message. The reason flows unchanged from the server SFS error
(`XrdOfs::Emsg` → `fsError` → `XrdXrootdMonFile::OpenErr`/`setCloseErr`) through
the collector (`fillError`), so the reason itself needed no production change.

**Open-failure reporting gaps closed.** Some failed opens never reached the
`fsError` path that emits the terminal `isError` record, so they were invisible
to collectors: (1) opens denied *before* the filesystem open in `do_Open` — the
file-lock manager's `kXR_FileLocked` (a second writer) and the out-of-memory
branches, which call `Response.Send` directly; and (2) opens that complete
*asynchronously*, reported through `XrdXrootdCallBack::sendError` rather than
`fsError`. Both now emit `XrdXrootdMonFile::OpenErr` (made a no-op when fstat
monitoring is off, via a `repBuff` guard, so it is safe to call from the
Monitor-less callback). The `kXR_FileLocked` case is exercised end-to-end by the
`xrdopen-denied` driver in the `XRootD::moncollect` test (`error_category:"open"`,
`error_code:3003`, `…is already opened by 1 writer; open denied.`); the async
branch mirrors the synchronous one and shares the same `OpenErr`/collector code
(triggering a *deferred* open failure needs an MSS/proxy backend not present in
the test harness).

Not covered: redirect terminal reports (already on the legacy `r` stream). Also
still not on the wire: a client-advertised `client.site` (only carried by
free-form appinfo by convention). `is_local` (LAN/WAN) is derived in the
collector by comparing client/server domains (see the completed items below).

#### Current status & next steps

**Status.** Tier 1 **and** Tier 2 are complete, plus all the Tier-1-adjacent
collector polish (items 2–5 below). The collector now reports authoritative
success/failure (`operation_state`) with error category/code/message for failed
opens and failed closes, with one consistent schema across every document type.
The `XrdMonCollectTests` suite (now including the failed-open, aborted-close, and
successful-close cases) passes, as does the `XRootD::moncollect` end-to-end
integration test.

Completed since the initial Tier 1 landing:

- **Schema consistency across all document types.** `session_end` (`EmitDisc`),
  `server_ident` (`DecodeIdent`), `frm` (`DecodeFrm`), the `t`-stream traces
  (`DecodeTStream`), `r`-stream redirects (`DecodeRStream`), and the `gstream`
  forward doc were migrated off the original flat keys onto the same nested
  `server.*`/`client.*`/`user.*`/`file.*` namespacing. Two shared helpers
  (`fillServer`, `fillClient`) now build those objects so every emitter stays in
  lock-step.
- **`is_local` LAN/WAN heuristic.** `transfer.is_local` is set when the client
  and the reporting server share a registered domain (omitted when either is an
  IP literal or the server host is unknown); it also drives
  `xrootd_collector_locality_transfers_total{server,locality}`.
- **OpenSearch sink shape.** `--os-datastream` switches the `_bulk` action to
  `create` for append-only data streams; a committed ECS-style composable index
  template (`opensearch-template.json`) maps the dotted fields explicitly (IPs
  as `ip`, byte counters as `long`, identifiers/strings as `keyword`).
- **Additional output sink.** A dependency-free `--forward host:port` TCP NDJSON
  sink (`XrdMonForward`) feeds buffering frontends (Logstash/Fluentd/Vector or a
  broker bridge), with lazy reconnect and a cool-down; sinks now fan out freely
  and stdout is only the no-sink fallback.

**Next steps.** The headline WLCG data gap is closed (failed opens, failed
closes, and mid-transfer read/write errors are all reported), and the two
remaining wire-level follow-ups below are now done as well. What is left is the
larger `XrdMetrics` registry migration ("Second iteration").

**Redirect terminal reports (DONE).** Redirect (`r`-stream) records, under
`--redirects`, are now emitted in the same `type:"transfer"` concluded-operation
schema as closes and errors: `transfer.operation_state` is `"Redirected"`, the
triggering `transfer.operation` is kept (e.g. `open-read`/`stat`), and the
destination travels under a `redirect` object (`kind`, `target_host`,
`target_port`) with the redirected `file.lfn`. A redirect concludes the
operation from the redirector's point of view; the data server that ultimately
serves the file emits its own `Successful`/`Failed` close. Redirects are also
counted in `xrootd_collector_redirects_total{server,kind}` regardless of the
flag. Covered by the `XrdMonCollect.RedirectStreamDecoded` unit test.

**Client-advertised `client.site` (DONE).** A client with `XRDSITE` (or
`XRD_SITE`, which overrides it) in its environment now advertises that site on
the wire: XrdCl imports it into the `ClientSite` env (`XrdClDefaultEnv`) and
appends `xrd.site=<site>` to the login CGI (`XrdClXRootDTransport::GenerateLogIn`)
only when set, so the login string is unchanged for everyone else. The server
(`do_Login`) folds it into the user-map appinfo as `&S=<site>` (a free key,
ignored by older collectors), and the collector parses it (`UserInfo::site`,
`cgiVal(text,"S")`) into `client.site` via the shared `fillClient` helper. This
is the client analogue of `server.site` (the reporting server's `XRDSITE` from
the `=` ident). Covered by the `Transfer.ClientSiteAdvertised` unit test and
end-to-end in `XRootD::moncollect` (`XRD_SITE=CLIENT-TEST-SITE`).

**VO surfacing (DONE, end-to-end).** The auth-derived VO path — gsi extracts a
VOMS attribute certificate into `XrdSecEntity.vorg` (`XrdSecProtocolgsi` →
`XrdVomsFun`), the server emits it in the MAPUSER record's CGI tail (`&o=`/`&r=`/
`&g=` in `XrdXrootdProtocol::MonAuth`, gated on `xrootd.monitor … auth`), and the
collector parses it into `user.vo`/`role`/`groups` plus
`xrootd_collector_vo_transfers_total{server,vo}` — is now exercised end-to-end by
the `XRootD::moncollect` test **when the VOMS plug-in is built**. Rather than
drive a live VOMS service, the test mints a fake VOMS proxy with `voms-proxy-fake`
(signed by the test host cert, trusted via a generated `vomsdir/<vo>/*.lsc`), runs
the server with gsi + `-vomsat:extract -vomsfun:libXrdVoms.so`, and asserts the
real `XrdVomsFun` extraction (`retrieval successful`) flows through to
`"vo":"dteam"`/`"role":"production"` on the transfer document. The test adds a
build/test dependency on the voms client tools (`voms-clients-cpp` for RPM,
`voms-clients` for Debian); on non-VOMS builds the moncollect config is unchanged.
No production C++ change was needed — the wire/extraction/collector path already
existed; this closes its test-coverage gap.

**Co-located server hostname (DONE).** When `xrdmoncollect` runs on the same host
as the server it monitors, the monitor datagrams arrive from the loopback address,
so `server.hostname`/`server.name` showed the literal `::1` until a `=` ident with
a real host arrived (and stayed numeric if the ident host was empty or itself an IP
literal). `ServerFor` now substitutes the collector's local FQDN
(`XrdNetUtils::MyHostName`) for a loopback source; the FQDN is resolved at most
once for the whole process (`LocalHost()`, cached) and reused for every loopback
incarnation. `fillServer` prefers a non-IP `=` ident host, else the resolved
name, else the numeric IP. Only the loopback case is resolved — a remote server
self-identifies on the `=` stream, and a blocking reverse-DNS lookup of an
arbitrary source IP would stall the single-threaded UDP receive loop and drop
packets. On by default; `--no-resolve` opts out. Unit-tested and asserted in
`XRootD::moncollect`.

**SciTags activity/VO mapping (DONE).** The one remaining WLCG data gap — a human
*activity* tag — is closed (conditionally) without a wire change. WLCG asks for
"Analysis/Production/…"; XRootD only carries the SciTags **numeric**
`experiment_id`/`activity_id` (`U`/MAPUEAC). With `--scitags <file>` pointing at a
SciTags registry (scitags.org schema: `experiments[].{expId,expName,activities[].
{activityId,activityName}}`), the collector maps those ids to `activity.experiment`/
`activity.activity` names and uses the experiment name as a `user.vo` fallback (only
when neither the `T` token nor the auth `&o=` supplied a VO). The numeric ids are
always emitted, so the field is present with or without the registry; a missing
source is warned about and ignored. `LoadScitags` parses via the bundled
`XrdOucJson.hh`. The `--scitags` source is a local file **or** an `http(s)://`
URL (e.g. `https://www.scitags.org/api.json`); a URL is re-fetched in a
background thread every `--scitags-refresh` seconds (default 3600) so a
long-running collector tracks the published registry, with the map swap
mutex-guarded against the decode loop (`LoadScitagsJson`) and a failed re-fetch
keeping the current registry. Unit-tested (`ScitagsRegistryMapsActivityAndVo`,
`ScitagsVoYieldsToToken`, `ScitagsNumericWithoutRegistry`,
`ScitagsMissingFileReturnsFalse`, `ScitagsJsonReloadReflectsUpdate`,
`ScitagsJsonBadInputKeepsRegistry`); the URL fetch + refresh path is smoke-tested
live against scitags.org.

---

# Second iteration — next-generation `XrdMetrics` registry

The phase-1 `XrdMetrics` prototype (a flat `XrdMetricsRegistry` that formats
labels per lookup and hard-codes the Prometheus text output) proved the concept
but did not scale to "large metric count, frequent scrape." A design review
(captured in `xrootd-metric-system.md`) settled a cleaner, faster,
format-agnostic architecture. This iteration implements that architecture as a
new, self-contained system under `src/XrdMetrics/`, namespace **`XrdMetrics`**,
compiled into `XrdUtils`.

It is **built alongside** the prototype, not in place of it: the flat
`XrdMetricsRegistry` is still consumed by the server wiring (`XrdXrootdStats`,
`Xrd/*`, `XrdHttpMetricsExporter`) and by `xrdmoncollect`, so it stays (deprecated)
until those are migrated onto the new system in a later step. The legacy
`XrdStats`/`XrdMonitor` XML/JSON path is untouched.

## Architecture (Registry → MetricGroup → Family → series)

Naming context flows **down at registration time**; iteration flows **down at
scrape time**. Labels are stored in three tiers by mutation profile:

- **Variable label names** — fixed per family, stored once (`LabelSchema`).
- **Const labels** — registry-global (e.g. an instance name) plus per-family;
  immutable.
- **Variable label values** — the only per-series differentiator and the child
  cache key (`LabelValues` + FNV-1a hash).

Because everything feeding a series' label rendering is immutable once the
series exists, each series builds its Prometheus text prefix
(`name{const…,var…} ` with a trailing space) **once**, in `SeriesLabels`. A
scrape is then, per series, a `memcpy` of the cached prefix plus one typed
number append plus a newline. The mutate hot path (`++`, `+=`, `=`) is a
lock-free relaxed atomic; the only lock on the update path is a per-family
`shared_mutex` taken the first time a label combination is seen (callers cache
the returned handle and hit it lock-free thereafter). Scraping snapshots
pointers under a brief read lock and serializes outside it, so it never blocks
producers.

Serialization goes through an **`ISerializer` visitor** with typed `series()`
overloads (`uint64_t`/`int64_t`/`double`), so a counter's value is never coerced
through `double`, and a new output format is a new subclass — never a change to
families or instruments. `MetricKind` carries the counter-vs-gauge distinction
structurally (for a future OTel `Sum` vs `Gauge`).

## Files

| File | Role |
|------|------|
| `XrdMetricsValue.hh` / `XrdMetricsSerialize.cc` | `appendValue` for u64/i64/double; the double path uses `std::to_chars` when the build probe `HAVE_FLOAT_TO_CHARS` is set (libstdc++ ≥ GCC 11) and a locale-guarded `snprintf("%.17g")` otherwise (AlmaLinux 8 / GCC 8). Integers always use integer `to_chars` (GCC 8+), and non-finite values render as `+Inf`/`-Inf`/`NaN`. |
| `XrdMetricsLabels.hh` | `LabelSchema`, `LabelContext`, `LabelValues`+hash, `SeriesLabels` (cached `prometheusPrefix()` + structured `forEachLabel()`), `joinName`, name/label validators. |
| `XrdMetricsInstrument.hh` | `MetricKind`; `Counter` (uint64, increment-only, monotonic by type); `Gauge<int64_t/double>` (set/`+=`/`-=`/`++`/`--`; integral `fetch_add`, double CAS loop for GCC 8); `IntGauge`/`FloatGauge`; `Histogram` (fixed bucket bounds, lock-free `observe`); `Summary` (lock-free count + sum, no quantiles). Atomics explicitly value-initialized (pre-C++20 UB guard). |
| `XrdMetricsSerializer.hh` | `ISerializer` seam + `PrometheusTextSerializer` (writes into a caller-owned reusable buffer). |
| `XrdMetricsFamily.hh` | `IFamily`; `LabeledFamily<Child>` two-tier child cache with a cardinality cap (over-cap label sets fold into one overflow series). |
| `XrdMetricsRegistry.hh` | `Registry` (prefix + frozen global labels + groups) and `MetricGroup` (subsystem factories `counter`/`intGauge`/`floatGauge` resolving `prefix_subsystem_name` once); process-wide `XrdMetrics::Default()`. |

A CMake probe in `cmake/XRootDSystemCheck.cmake` (`check_cxx_source_compiles`
calling `std::to_chars(double)`) defines `HAVE_FLOAT_TO_CHARS`, so the split
lands on the distro boundary (Alma 9/GCC 11 → `to_chars`, Alma 8/GCC 8 →
`snprintf`) without devtoolset and with no new ABI surface.

## Instruments and serializers

- **Instruments:** Counter, Gauge (int64/double), Histogram, Summary, plus
  read-only **observed** metrics whose value comes from a `std::function` reader
  at scrape time (the typed, first-class replacement for the prototype's
  `AddRefCounter`/`AddRefGauge`). The Summary is quantile-less by design: a
  lock-free running count + sum (`observe(v)` is two relaxed atomics, the sum via
  the same portable CAS loop as the double Gauge), rendered as `_sum`/`_count`
  under `# TYPE … summary`. Client-side quantiles were deliberately not added —
  they would force a lock on the hot path and yield non-aggregatable series; for
  server-side quantiles use a Histogram.
- **Population patterns:** counters are *reversed* (the metric owns the atomic;
  the legacy emitter reads it via `value()`); live control-flow state stays
  owned by its subsystem and is *observed* read-only. Callers that build
  labelled metrics on the fly use the get-or-create `counterSeries` /
  `gaugeSeries` / `histogramSeries` helpers (family deduplicated by name).
- **Serializers:** Prometheus text only, behind the abstract `ISerializer` seam
  (`MetricKind` + typed `series`/`histogram`) so OTel JSON and the existing
  XRootD XML/JSON can be added later as sibling subclasses. A Prometheus-text
  collector escape hatch (`Registry::addTextCollector`) bridges the legacy
  `XrdMonRoll` plugin counter sets.

## Migration status

The phase-1 prototype (`XrdMetricsRegistry`) has been **removed**: every
consumer now uses the new system —

- **Server summary metrics** — process/CPU and server identity, link, poll,
  buffer-pool, and the xrootd protocol op/login/signature/byte counters
  (`XrdStats`, `XrdLink`, `XrdPoll`, `XrdBuffer`, `XrdXrootdStats`); the
  scheduler is fully migrated (counters reversed, live gauges observed).
- **`/metrics` endpoint** (`XrdHttpMetricsExporter`) serves only the new registry,
  and can additionally push it (Pushgateway text / OTLP JSON).
- **`xrdmoncollect`** aggregate sink uses the new registry (its own empty-prefix
  `Registry`).
- **HTTP plugin counters** — `XrdHttpMon` (the only `XrdMonRoll` user) now reports
  per-method request and per-status response counts natively as
  `xrootd_http_requests_total{method=…}` / `xrootd_http_responses_total{code=…}`
  in `Default().group("http")`, registered unconditionally and incremented via
  cached series handles. The phase-1 `XrdMonitor::FormProm` text bridge and its
  `XrdStats` text-collector registration were removed with it. `XrdMonRoll` itself
  is left untouched (no remaining users; a candidate for later removal).

The legacy `XrdStats`/`XrdMonitor` XML/JSON summary output is untouched.

## Tests

`tests/XrdMetricsTests/XrdMetricsRegistryTests.cc` covers value formatting,
counter/gauge operators and the double CAS path, label prefix order/escaping and
the structured `forEachLabel`, name validation, the cached family handle, the
cardinality cap, histograms (buckets/sum/count, labelled `le`), summaries
(lock-free count + sum, labelled and dynamic series, no quantile series),
observed metrics
(single- and multi-series), the dynamic `*Series` get-or-create with family
dedup and type-mismatch errors, group/registry composition, `Default()`, exact
Prometheus output, buffer reuse, and concurrent increments. Server-side and
collector behaviour is exercised by `XrdSchedulerStatsTests`,
`XrdXrootdStatsTests` (legacy XML byte-stability) and the `XrdMonCollectTests`
suite, plus the server integration tests.

## Serializers

Three `ISerializer` implementations share one registry traversal:

- `PrometheusTextSerializer` — the exposition text scraped at `/metrics`.
- `OtelJsonSerializer` — an OTLP/JSON `ExportMetricsServiceRequest`
  (resourceMetrics → scopeMetrics → metrics) ready to POST to an OTLP/HTTP
  receiver. Counter → monotonic cumulative Sum, Gauge → Gauge, Histogram →
  cumulative Histogram with de-cumulated `bucketCounts`, Summary → OTLP Summary
  (count + sum, empty `quantileValues`); 64-bit ints are JSON strings, non-finite
  doubles use the NaN/Infinity tokens, and every point carries the scrape time.
  Document framing uses the `begin()`/`end()` hooks
  `Registry::serialize()` calls around the traversal (no-ops for Prometheus).
- `MetricSnapshot` — not an output format but a by-name value collector
  (keyed by the `name{labels}` series identity) that lets a consumer pull
  values for a layout outside the flat metric model.

## Driving the legacy stats from the registry

Goal: make the new registry the single source of truth and reproduce the
pre-existing `XrdStats` `<stats id=...>` XML (and later the JSON) from it, so a
future configuration directive can flip which counters register — changing the
exposed names — while keeping full backward compatibility for the old reports.
`XrdHttpMon` is the exception already fully migrated (its counters were only
released recently and have few users).

Approach (chosen with the user): a legacy-only renderer (`XrdStatsLegacy`) hard
-codes each legacy block's element layout and pulls values from a
`MetricSnapshot` by native metric name (reading a source directly where the
representation differs, e.g. proc microseconds vs the new seconds). All legacy
-schema knowledge stays in that renderer; the XrdMetrics core stays clean.

Done:

- The `info`, `proc`, `buff`, `link`, `poll` and `sched` blocks render through
  `XrdStatsLegacy`, and `XrdStats::GenStats` now snapshots the registry once and
  produces them from it (the former `InfoStats`/`ProcStats` members are gone; a
  `do_sync` report still flushes the deferred per-link counts first; the buff
  block splices in the nested buffer-XL fragment via `XrdBuffManager::xlStats`).
- The `<stats id="xrootd">` protocol block renders through
  `XrdStatsLegacy::Xrootd`; `XrdXrootdStats::Stats` snapshots the registry and
  emits it from there, still appending the filesystem block. The existing
  byte-stability test exercises the full round-trip unchanged.

- The `<stats id="ofs">` filesystem block renders through `XrdStatsLegacy::Ofs`.
  This was the first block needing **new metrics registered first** (group
  `ofs`): `XrdOfsStats::RegisterMetrics` (called from its constructor) observes
  the `Data` fields — open-file counts by mode and the handle count as gauges,
  the rest as counters — and `XrdOfsStats::Report` now snapshots and renders
  through them. The OSS block that `XrdOfs::getStats` appends afterward is still
  unchanged.

Remaining:

- The nested `<stats id="oss">` block (`XrdOfsOss->Stats`) is intentionally left
  as-is. Unlike the other blocks it is a dynamic enumeration of disk paths and
  space groups whose values are live filesystem queries (not internal counters),
  with per-entry indices and string identities. Per-path/per-group gauges would
  risk a cardinality explosion in the registry, so OSS space reporting is a
  better fit for the `xrdmoncollect` collector later, not the in-process
  registry. The legacy oss block keeps rendering through `XrdOssSys` directly.
- The configuration directives that flip which counters register (and thus the
  exposed names).

## Next steps (later iterations)

1. **Configuration directives** that flip which counters register (and thus the
   exposed metric names): a compat mode that keeps registering the legacy
   counters so the old `<stats id=…>` reports render unchanged, and a new-only
   mode that registers just the native names. This is the final piece that makes
   the registry the single source of truth with config-controlled naming.
   (**ACTUALLY ALREADY DONE**)

2. **OSS space reporting** via `xrdmoncollect` rather than the in-process
   registry (see the OSS deferral above), to avoid a per-path/per-group
   cardinality explosion.

3. **Client-side metrics** for batch jobs.


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

Deferred to later work (documented but not implemented in this phase):
- UDP / Pushgateway-style push of the Prometheus format.
- An external collector that decodes the binary `u`/`d`/`t`/`f` event streams,
  correlates them, and emits aggregated per-file-close events.

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

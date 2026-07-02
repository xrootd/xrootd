# XrdMetrics

`XrdMetrics` is XRootD's native instrumentation library: a small, lock-free,
Prometheus- and OpenTelemetry-aware metrics system built into `XrdUtils` and
therefore linked by every component and plugin. Subsystems declare counters,
gauges, histograms and summaries against a process-wide registry; the
`XrdHttpMetricsExporter` plugin serves or pushes them as Prometheus text or
OTLP/JSON.

This document is developer-focused: how to instrument code, how the configuration
directives interact with the classes, and the lifetime rules you must respect.

## Object model

```
CollectorRegistry   process-wide directory of Collectors (one scrape/push)
  └─ Collector      a name prefix + frozen global labels + its Subsystems
       └─ Subsystem a named group ("sched", "cms", "http", ...) that makes families
            └─ Family     one metric name; owns its label-keyed series
                 └─ Series/Instrument   Counter / Gauge / Histogram / Summary
```

Lifetime is **strictly nested**: a `Collector` outlives its `Subsystem`s, which
outlive their families, which outlive their series. Back-pointers rely on this, so
you never heap-allocate a `Collector` that outlives the code using it — use the
process-wide default:

```cpp
XrdMetrics::Collector& c = XrdMetrics::Default();   // prefix "xrootd", auto-registered
```

`Default()` returns the singleton `Collector` with prefix `xrootd`; it joins the
`CollectorRegistry` on first use, so anything you register on it is scraped.

All examples below use `namespace XrdMetrics`.

## Quick start

A subsystem author grabs a `Subsystem`, creates instruments once (they are returned
by reference and cached — hold the reference, do not re-look-up on the hot path),
then updates them. From `src/Xrd/XrdScheduler.cc`:

```cpp
#include "XrdMetrics/XrdMetricsInstrument.hh"   // Counter, Gauge, Histogram, Summary
#include "XrdMetrics/XrdMetricsRegistry.hh"     // Collector, Subsystem, CollectorRegistry

XrdMetrics::Subsystem& sched = XrdMetrics::Default().subsystem("sched");

m_Jobs    = &sched.counter<std::uint64_t>("jobs_total", "jobs scheduled");
m_TCreate = &sched.counter<std::uint64_t>("threads_created_total", "worker threads created");

// ... later, on the hot path (lock-free):
++*m_Jobs;
```

The full metric name is `<prefix>_<subsystem>_<name>`, e.g. `xrootd_sched_jobs_total`.
Metric and label names are validated at creation (`[a-zA-Z_:][a-zA-Z0-9_:]*` /
`[a-zA-Z_][a-zA-Z0-9_]*`); an invalid name throws `std::invalid_argument`.

## Metric types (instruments)

All instruments update through relaxed atomics — no locks on the update path — and
are safe to increment concurrently from many threads.

### Counter — monotonic (`uint64_t` or `double`)

```cpp
Counter<std::uint64_t>& c = collector.subsystem("ops").counter<std::uint64_t>("requests_total");
++c;         // pre-increment
c++;         // post-increment
c += 5;      // add
c.value();   // read  -> 7

Counter<double>& cpu = collector.subsystem("proc").counter<double>("cpu_seconds_total", "cpu time");
cpu += 1.5;  // accumulates fractional amounts
```

A counter has no `=`, `-=` or reset — monotonicity is enforced by the type.

### Gauge — up/down (`int64_t` or `double`)

```cpp
Gauge<std::int64_t>& g = collector.subsystem("sched").gauge<std::int64_t>("threads");
g = 8;  g += 2;  g -= 3;  ++g;  --g;   // -> 7
```

### Histogram — bucketed observations

Buckets are given as upper bounds; a `+Inf` bucket is added implicitly and the
serializer emits cumulative `_bucket`, plus `_sum` and `_count`:

```cpp
auto& h = collector.subsystem("io").histogram("size_bytes", {1, 2, 5}, "io sizes");
h.observe(0.5);  h.observe(1.5);  h.observe(3);  h.observe(10);
// xrootd_io_size_bytes_bucket{le="1"} 1 ... {le="+Inf"} 4 ; _sum 15 ; _count 4
```

### Summary — sum + count, no quantiles

Client-side quantiles are intentionally omitted so series stay aggregatable across
instances; a summary emits only `_sum` and `_count`:

```cpp
auto& s = collector.subsystem("xrootd").summary("request_bytes", "request sizes");
s.observe(100);  s.observe(250);  s.observe(50);
// xrootd_xrootd_request_bytes_sum 400 ; _count 3
```

## Labels and families

To attach variable labels, create a *family* and pick a series with `.labels({...})`
(positional, in declared order). The same values return the same cached handle:

```cpp
auto& fam = collector.subsystem("ops")
    .counterFamily<std::uint64_t>("requests_total", "requests", {"verb"} /*var labels*/);

fam.labels({"open"}) += 3;
fam.labels({"read"}) += 5;
fam.labels({"open"});   // same handle as before
```

`histogramFamily`, `summaryFamily` and `gaugeFamily` work the same way.

Labels come from three sources and are always emitted in this order — **global**
(registry-wide), then **family const**, then **variable**:

```cpp
Collector collector("xrootd", {{"instance", "h1"}});          // global label
auto& fam = collector.subsystem("ops")
    .counterFamily<std::uint64_t>("requests_total", "", {"verb"}, {{"proto", "xroot"}});
fam.labels({"open"});
// xrootd_ops_requests_total{instance="h1",proto="xroot",verb="open"}
```

The Prometheus prefix string is built **once** per series at creation and reused on
every update; values are escaped for you.

### Cardinality cap

Pass `maxKids` to a family to bound its distinct label combinations. Combinations
past the cap fold into a single `__over_cardinality_limit__` overflow series rather
than growing memory without bound:

```cpp
auto& fam = collector.subsystem("g")
    .counterFamily<std::uint64_t>("c", "", {"k"}, {} /*const labels*/, /*maxKids=*/2);
fam.labels({"1"});  fam.labels({"2"});   // distinct
fam.labels({"3"});  fam.labels({"4"});   // both -> __over_cardinality_limit__
```

## Native vs observed metrics

Two ways to expose a value:

- **Native (owned) instruments** — `counter()`, `gauge()`, `histogram()`,
  `summary()`. *The metric is the storage*: you increment the atomic directly.
  Prefer this.
- **Observed instruments** — `observeCounter<T>()` / `observeGauge<T>()` wrap a
  value **owned elsewhere** via a reader callback invoked at scrape time. The
  callback must be cheap and thread-safe.

Both appear side by side in `XrdScheduler`: the event tallies are owned counters,
while live scheduler state (thread counts, queue depth) stays a plain `int` and is
surfaced read-only:

```cpp
sched.observeGauge<std::int64_t>("threads", "worker threads")
     .add({}, [this]{ return (int64_t)num_Workers; });        // read at scrape time
```

`.add({labels}, reader)` chains, so one observed family can carry many series:

```cpp
collector.subsystem("ops").observeCounter<std::uint64_t>("total", "operations", {}, {"op"})
    .add({"open"}, [&]{ return open; })
    .add({"read"}, [&]{ return read; });
```

Rule of thumb: use an owned instrument unless the value is a pre-existing
control-flow variable you cannot replace with a counter — then observe it.

## Dynamic series (`*Series`)

When label *values* are discovered at runtime (e.g. remote-reported data in
`xrdmoncollect`), use the get-or-create helpers. They deduplicate the family by
name and are capped at `kDynamicSeriesCap` (8192) to contain untrusted cardinality:

```cpp
auto& sub = collector.subsystem("");
++sub.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","stage"}});
sub.gaugeSeries("xrootd_collector_active", "active", {{"server","s1"}}) = 4;
sub.histogramSeries("xrootd_collector_sz", "sizes", {10,100}, {{"op","read"}}).observe(50);
```

Requesting an existing name with a different kind throws `std::invalid_argument`.

## Serialization

A `Serializer` is the output seam; families call back into it. Pass one to
`Collector::serialize()`:

```cpp
std::string out;                                   // caller owns and may reuse the buffer
PrometheusTextSerializer ser(out);
collector.serialize(ser);                          // Prometheus exposition text
```

```cpp
OtelJsonSerializer ser(out, "xrootd", {{"service.instance.id", "node-7"}});
collector.serialize(ser);                          // OTLP/JSON, resourceMetrics/scopeMetrics, schema v1.26.0
```

The `PrometheusTextSerializer` is allocation-free in steady state; reuse the same
buffer across scrapes (`out.clear()` between calls). `OtelJsonSerializer` requires
the framing that `serialize()` provides — do not append raw text into the same
buffer.

`MetricSnapshot` collects the registry into a name→value map for non-flat legacy
outputs (e.g. the `<stats>` XML), keyed by the series identity `name{labels}`:

```cpp
MetricSnapshot snap;
collector.serialize(snap);
snap.getInt("xrootd_sched_jobs_total");                       // 7
snap.getInt("xrootd_proc_cpu_seconds_total{mode=\"user\"}");  // labelled series
```

### Multiple registries

`CollectorRegistry` walks every registered `Collector` in one pass (one OTLP
resource each). This is how a plugin such as EOS contributes its own prefixed
`Collector` alongside `xrootd`:

```cpp
CollectorRegistry::instance().add(myCollector);   // idempotent; destructor auto-removes
CollectorRegistry::instance().serialize(ser, filter);
```

`Collector::serializeBody(ser, filter)` emits without framing and applies a
`GroupFilter` — a predicate on the subsystem name — to select which subsystems appear.

## Configuration interaction

Directives are parsed by `XrdMetrics::Config` (`metrics.*`, plus `all.role`):

| Directive | Meaning | Default |
|-----------|---------|---------|
| `metrics.enable <bool>` | master on/off switch | on |
| `metrics.subsystems [+s] [-s] s …` | per-subsystem filter (see below) | all on |
| `metrics.label <key> <val>` | extra global label (repeatable) | — |
| `metrics.path <path>` | HTTP endpoint path | `/metrics` |
| `metrics.instance <name>` | `instance` label | hostname |
| `metrics.pushurl <url>` | Pushgateway base URL (`""` = off) | off |
| `metrics.pushinterval <sec>` | Pushgateway period | 30 |
| `metrics.pushjob <name>` | Pushgateway `job` label | `xrootd` |
| `metrics.otelurl <url>` | OTLP/HTTP metrics URL (`""` = off) | off |
| `metrics.otelinterval <sec>` | OTLP push period | 30 |
| `all.role <role>` | source for the `role` global label | — |

`metrics.subsystems` token syntax: `-name` denies a subsystem, `+name` (or a bare
`name`) allows it. When any allow token is present the filter is default-deny —
only listed subsystems are emitted.

The essential mental model:

> **Registration is unconditional; enable/disable is a serialize-time filter.**

Subsystems always create their families and always update their (cheap, atomic)
counters. `metrics.enable` and `metrics.subsystems` only decide what the exporter
*emits*, via a `GroupFilter` built from `Config::subsystemEnabled()` and handed to
`serialize()`. Disabling a subsystem never removes instrumentation from the code
path, so toggling it has no correctness impact and near-zero cost.

`Config::Load()` is idempotent and runs **early** in daemon startup (e.g.
`XrdConfig.cc`, before any subsystem registers a family) so that the global labels
it computes — program name, `role`, and `metrics.label` entries — can be frozen
onto the default registry in time. See the lifetime rule below.

## Initialization and lifetime constraints

- **Global labels must be frozen before the first family is created.** The prefix
  string of every series bakes in the global labels, so `setGlobalLabels()` (and
  `Config::Load()`, which calls it) succeeds only while the registry has no families
  yet. Set them at startup, not after instrumentation has begun.
- **Respect the nesting.** `Collector` ⊃ `Subsystem` ⊃ Family ⊃ Series; children
  hold back-pointers to parents. Do not create a `Collector` with a shorter lifetime
  than the code registering into it — use `XrdMetrics::Default()` for process-global
  metrics.
- **Hold instrument references.** Factory methods return a stable reference to a
  cached instrument; look it up once at registration and keep the pointer/reference.
  Re-looking-up on the hot path defeats the purpose.
- **`CollectorRegistry::instance()` is a leaked singleton** so it outlives static
  `Collector`s during shutdown; a `Collector` unregisters itself in its destructor.
- **Thread-safety.** Instrument updates are lock-free. Adding a *new* series to a
  family (first `.labels({...})` for a value) takes a brief write lock; scrapes take
  a read lock and serialize a stable snapshot. Observed-metric readers run during a
  scrape and must be cheap and thread-safe.

## Where to look

- Instruments: `XrdMetricsInstrument.hh` · Families: `XrdMetricsFamily.hh`
- Registry (Collector/Subsystem/CollectorRegistry): `XrdMetricsRegistry.hh`
- Labels: `XrdMetricsLabels.hh` · Serializers: `XrdMetricsSerializer.hh`
- Configuration: `XrdMetricsConfig.hh`
- Worked examples: `tests/XrdMetricsTests/` and `tests/XrdPfcTests/XrdPfcMetricsTests.cc`

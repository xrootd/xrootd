# XrdHttpMetricsExporter

An `XrdHttpExtHandler` that exposes the XRootD metrics registry (`XrdMetrics`)
to monitoring backends. It can be **scraped** over HTTP in the Prometheus text
exposition format and, optionally, **pushed** in the background to:

- a **Prometheus Pushgateway** (PUT, text exposition format), and/or
- an **OTLP/HTTP** receiver (POST, OpenTelemetry OTLP/JSON).

Either, both, or neither push target may be active.

## Loading

```
http.exthandler metrics [+notls] libXrdHttpMetricsExporter.so [/metrics]
```

The optional argument overrides the served request path (default `/metrics`).
Like any HTTP external handler it requires TLS unless `+notls` is given.

## Pull (scrape)

`GET /metrics` returns the current metrics with
`Content-Type: text/plain; version=0.0.4`. Point a Prometheus scrape job at the
server's HTTP port. The pull endpoint always serves the Prometheus text format;
OTLP is push-only.

## Push (optional)

When a push URL is configured the plugin starts a background thread per target.
Pushing requires the plugin to be built with libcurl (linked automatically when
found).

- **Prometheus Pushgateway** — periodically PUTs the text exposition format to
  `<pushurl>/metrics/job/<job>/instance/<instance>`.
- **OTLP/HTTP** — periodically POSTs an OpenTelemetry `ExportMetricsServiceRequest`
  (OTLP/JSON) to the configured receiver URL. The server identity is carried in
  the OTLP Resource as `service.name=xrootd` and `service.instance.id=<instance>`.

## Configuration

The `metrics.*` directives are parsed once by the server core (early, so the
global labels are frozen before any subsystem registers a metric) into a
process-wide configuration that this plugin reads. They therefore apply whether
metrics leave via the pull endpoint, the push paths, or not at all.

```
# Collector-level (process-wide)
metrics.enable       yes|no     # master switch (default yes)
metrics.label        <k> <v>    # constant global label on every series (repeatable)
metrics.subsystems   <list>     # per-subsystem enable/disable (see below)

# Exporter
metrics.path         <path>     # served request path (default /metrics)
metrics.instance     <name>     # instance label / resource id (default hostname)
metrics.pushurl      <url>      # Pushgateway base URL; enables Pushgateway push
metrics.pushinterval <seconds>  # Pushgateway push period (default 30)
metrics.pushjob      <name>     # Pushgateway job label (default xrootd)
metrics.otelurl      <url>      # OTLP/HTTP metrics URL; enables OTLP push
metrics.otelinterval <seconds>  # OTLP push period (default 30)
```

### Global labels

`metrics.label k v` adds a constant label to every series (useful to identify a
cluster beyond the site name, e.g. `metrics.label cluster prod-eu`). Two labels
are auto-seeded and can be overridden: `program` (the daemon, `xrootd`/`cmsd`)
and `role` (the configured `all.role`). These distinguish the daemons and roles
without fragmenting metric names.

### Subsystem enable/disable

`metrics.subsystems` filters by registry group name **at serialize time** — the
counters still update; disabled subsystems are simply omitted from output:

```
metrics.subsystems -sched          # disable the sched subsystem
metrics.subsystems sched link       # allow-list: emit only these subsystems
metrics.subsystems +sched -proxy    # + allow, - deny (deny wins)
```

A bare or `+`-prefixed name forms an allow-list (only those subsystems are
emitted); a `-`-prefixed name is always denied. `metrics.enable no` turns
everything off.

The subsystem (group) names currently registered are: the base xrootd protocol
(empty group, flat `xrootd_*` names) plus `process`, `link`, `poll`, `sched`,
`buff`, `ofs`, `http` and `metrics` on a data server; `proxy` when running as a
proxy; `cache` when running as a cache; and `cms` in the cmsd clustering daemon.

### Multiple registries

The exporter aggregates **every** registry in the process-wide directory into one
scrape/push, not just the default `xrootd_*` registry. A plugin or a foreign
owner (e.g. EOS) can keep its own `XrdMetrics::Collector` and call
`XrdMetrics::registerRegistry()`; its series then appear alongside the others
(distinct name prefix for Prometheus, one OTLP `resourceMetrics` block per
registry carrying that registry's global labels).

Example:

```
http.exthandler metrics libXrdHttpMetricsExporter.so
metrics.label         cluster prod-eu
metrics.pushurl       http://pushgateway:9091
metrics.pushinterval  30
metrics.pushjob       xrootd
metrics.otelurl       http://otel-collector:4318/v1/metrics
metrics.otelinterval  30
```

For direct scraping, prefer the pull endpoint; you can add per-scrape labels via
the Prometheus job's `external_labels` too. The push paths are meant for
environments where the server cannot be scraped directly.

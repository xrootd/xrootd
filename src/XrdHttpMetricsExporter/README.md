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

### Caching and rate limiting

The pull endpoint protects itself against redundant work and abuse. Both apply
to the scrape path only (the push threads serialize independently):

- **TTL cache** (`metrics.scrapettl`, default 10 s) — the serialized body is
  reused across scrapes within the TTL window, so several Prometheus replicas
  scraping the same server do not each pay for a full registry walk. A cache
  miss serializes once under a mutex; concurrent waiters then hit the freshly
  populated cache (no thundering herd). Set `0` to serialize on every request.
- **Rate limiter** (`metrics.scraperatelimit`, default 100 req/s) — requests
  beyond the per-second cap get `HTTP 503` with `Retry-After: 1`. Set `0` to
  disable. The auth check (below) runs *before* the limiter, so unauthenticated
  probes cannot exhaust a legitimate scraper's allowance.

The endpoint reports on itself through counters in the `metrics` subsystem:
`xrootd_metrics_scrapes_total`, `..._scrapes_cache_hits_total`,
`..._scrapes_rate_limited_total`, and `..._scrapes_auth_failed_total`.

### Authentication

By default `/metrics` is open (like most Prometheus exporters). Setting any of
`metrics.requireauth`, `metrics.authtoken`, or `metrics.authpassword` turns on a
check that a request must pass in one of three ways, tried in order:

1. **Bearer token** — `Authorization: Bearer <token>` matching `metrics.authtoken`
   (constant-time compare). This is the simplest option for Prometheus: the admin
   generates a shared secret and hands it to the monitoring team as a scoped,
   long-lived scrape credential.
2. **HTTP Basic Auth** — `Authorization: Basic base64(user:pass)` whose password
   matches `metrics.authpassword` (the username is ignored).
3. **TLS client certificate** — any request carrying a client certificate whose
   DN was validated by XrdHttp (i.e. `SecEntity.moninfo` is populated) is admitted.

A request that satisfies none of the configured methods receives `HTTP 401` with a
`WWW-Authenticate` header offering both `Bearer` and `Basic`. `metrics.requireauth
yes` on its own admits only certificate-bearing clients (no shared secret).

> **Note on tokens.** HTTP external handlers are dispatched before XrdHttp's
> `Bridge::Login()`, so the XrdSec token stack (SciTokens, macaroons) and
> `XrdAccAuthorize` are **not** consulted for `/metrics`. `metrics.authtoken` is a
> plain shared secret compared by this handler, not a validated bearer token.
> Certificate DNs, however, *are* validated by XrdHttp during the TLS handshake.

Prometheus scrape config using a Bearer token:

```yaml
scrape_configs:
  - job_name: xrootd
    scheme: https
    authorization:
      credentials: <metrics.authtoken value>
    static_configs:
      - targets: ['xrootd-host:1094']
```

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
metrics.path           <path>     # served request path (default /metrics)
metrics.instance       <name>     # instance label / resource id (default hostname)
metrics.pushurl        <url>      # Pushgateway base URL; enables Pushgateway push
metrics.pushinterval   <seconds>  # Pushgateway push period (default 30)
metrics.pushjob        <name>     # Pushgateway job label (default xrootd)
metrics.otelurl        <url>      # OTLP/HTTP metrics URL; enables OTLP push
metrics.otelinterval   <seconds>  # OTLP push period (default 30)

# Scrape endpoint protection (pull path only)
metrics.scrapettl      <seconds>  # reuse serialized body within TTL (default 10; 0 = off)
metrics.scraperatelimit <n>       # max scrapes per second (default 100; 0 = unlimited)

# Scrape endpoint authentication (pull path only)
metrics.requireauth    yes|no     # require a credential to scrape (default no)
metrics.authtoken      <token>    # accept Authorization: Bearer <token>
metrics.authpassword   <password> # accept HTTP Basic Auth with this password
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

The exporter aggregates **every** Collector in the process-wide directory into one
scrape/push, not just the default `xrootd_*` Collector. A plugin or a foreign
owner (e.g. EOS) can keep its own `XrdMetrics::Collector` and register it with
`XrdMetrics::CollectorRegistry::instance().add()`; its series then appear alongside the others
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

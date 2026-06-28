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

Configuration directives (read from the main configuration file):

```
metrics.path         <path>     # served request path (default /metrics)
metrics.instance     <name>     # instance label / resource id (default hostname)
metrics.pushurl      <url>      # Pushgateway base URL; enables Pushgateway push
metrics.pushinterval <seconds>  # Pushgateway push period (default 30)
metrics.pushjob      <name>     # Pushgateway job label (default xrootd)
metrics.otelurl      <url>      # OTLP/HTTP metrics URL; enables OTLP push
metrics.otelinterval <seconds>  # OTLP push period (default 30)
```

Example:

```
http.exthandler metrics libXrdHttpMetricsExporter.so
metrics.pushurl       http://pushgateway:9091
metrics.pushinterval  30
metrics.pushjob       xrootd
metrics.otelurl       http://otel-collector:4318/v1/metrics
metrics.otelinterval  30
```

For direct scraping, prefer the pull endpoint and add site/cluster labels via
the Prometheus scrape job's `external_labels`. The push paths are meant for
environments where the server cannot be scraped directly.

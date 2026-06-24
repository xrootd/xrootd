# XrdHttpPrometheus

An `XrdHttpExtHandler` that exposes the XRootD metrics registry
(`XrdMetricsRegistry`) in the Prometheus text exposition format. It can be
**scraped** over HTTP and, optionally, **pushed** to a Prometheus Pushgateway.

## Loading

```
http.exthandler prometheus [+notls] libXrdHttpPrometheus.so [/metrics]
```

The optional argument overrides the served request path (default `/metrics`).
Like any HTTP external handler it requires TLS unless `+notls` is given.

## Pull (scrape)

`GET /metrics` returns the current metrics with
`Content-Type: text/plain; version=0.0.4`. Point a Prometheus scrape job at the
server's HTTP port.

## Push (optional, Pushgateway)

When a push URL is configured the plugin starts a background thread that
periodically PUTs the metrics to a Prometheus Pushgateway at
`<url>/metrics/job/<job>/instance/<instance>`. This requires the plugin to be
built with libcurl (linked automatically when found).

Configuration directives (read from the main configuration file):

```
prometheus.path         <path>     # served request path (default /metrics)
prometheus.pushurl      <url>      # Pushgateway base URL; enables pushing
prometheus.pushinterval <seconds>  # push period (default 30)
prometheus.pushjob      <name>     # job label (default xrootd)
prometheus.pushinstance <name>     # instance label (default hostname)
```

Example:

```
http.exthandler prometheus libXrdHttpPrometheus.so
prometheus.pushurl      http://pushgateway:9091
prometheus.pushinterval 30
prometheus.pushjob      xrootd
```

For direct scraping, prefer the pull endpoint and add site/cluster labels via
the Prometheus scrape job's `external_labels`. The push path is meant for
environments where the server cannot be scraped directly.

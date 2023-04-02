
Prometheus Exporter for XRootD
------------------------------

The Prometheus exporter for XRootD provides monitoring data about the XRootD
process using the OpenMonitoring / prometheus exposition format.

This is made available as a HTTP extension handler, allowing the admins running
the HTTP protocol to make the monitoring available from the `xrootd` daemon itself.


Configuring
===========

To configure, simply load the prometheus plugin in the configuration file:

```
http.exthandler xrdprometheus libXrdPrometheus.so
```

To control the verbosity of logging, use the `prometheus.trace` directive:

```
prometheus.trace all
```

The valid values for `prometheus.trace` are `all`, `debug`, `info`, `warning`,
and `error`; the default logging level is `warning`.

Path-based monitoring
=====================

By default, the Prometheus plugin will aggregate all transfer statistics under
a label of `path="/"`.  To have more detailed per-path monitoring, use the
`prometheus.monpath` directive:

```
prometheus.monpath /user/*
prometheus.monpath /data
```

Multiple directives are permissible and additive.  The wildcard will match any
directory prefix with the given wildcard.  So, if there are ongoing transfers
in `/user/frank` and `/user/jane`, then `/user/*` will match both prefixes.


# XRootD Cloud-Native Monitoring — WLCG Site Deployment Guide

**Pipeline:** XRootD servers → `xrdmoncollect` (shoveler → collector) → Grafana Alloy → Prometheus (metrics) + Loki *or* OpenSearch (logs/documents) + Tempo (traces) → Grafana (UI + alerting)

This guide covers the full monitoring pipeline, from the XRootD data servers
at a site all the way to the dashboards:

- **Part I** — the site side: configuring XRootD servers and deploying
  `xrdmoncollect` in shoveler and collector modes. Four deployment tracks:
  native packages (A), podman (B), docker compose (C), Kubernetes (D).
- **Part II** — the central monitoring backend (Alloy, Prometheus, Loki,
  Tempo, Grafana, optionally OpenSearch) as native packages on AlmaLinux 10.
- **Part III** — long-term storage for the container-based deployments,
  operations (token rotation, restarts, alerting), and end-to-end
  verification.

All hops between components authenticate with **bearer tokens** (section 0.3);
the one hop that cannot (UDP) is kept on localhost.

The backend part consolidates a working deployment, including corrections
found during rollout:

- Prometheus is installed **from EPEL with dnf** (v3.12 at time of writing),
  not from the upstream binary. dnf keeps it updated.
- The Tempo config targets **Tempo 3.0+** (the 2.x `ingester:`/`compactor:`
  blocks and flat `overrides:` format are rejected by 3.0).
- The Alloy OTLP receiver is **secured with TLS + bearer-token auth** for
  remote applications.
- Remote `node_exporter` hosts are covered (scrape-based, plus a push
  alternative).
- Section 17 adds OpenSearch (+ Data Prepper) as an alternative log backend
  to Loki.

## Table of contents

| Section | Content |
|---------|---------|
| 0 | Architecture, port map, bearer-token matrix |
| **Part I** | **XRootD servers and xrdmoncollect at the site** |
| 1 | Topology: shoveler and collector modes |
| 2 | XRootD server configuration |
| 3 | xrdmoncollect configuration |
| 4 | Track A — native packages (VMs / bare metal) |
| 5 | Track B — podman |
| 6 | Track C — docker compose (full stack on one host) |
| 7 | Track D — Kubernetes |
| **Part II** | **The central monitoring backend (AlmaLinux 10, native packages)** |
| 8 | System prep |
| 9 | Prometheus |
| 10 | Grafana repo + package install |
| 11 | Loki |
| 12 | Tempo |
| 13 | Alloy (secured OTLP ingest) |
| 14 | Grafana |
| 15 | Firewall |
| 16 | Remote node_exporter hosts |
| 17 | Alternative log backend: OpenSearch |
| **Part III** | **Storage, operations, verification** |
| 18 | Long-term storage for container deployments |
| 19 | Operations |
| 20 | End-to-end verification |
| — | Appendix: service cheat-sheet |

---

## 0. Architecture, port map, authentication

### 0.1 End-to-end architecture

Recommended topology — a shoveler on every XRootD node, one collector per
site:

```
each XRootD node                central collector host           monitoring host
┌──────────────────────────┐    ┌──────────────────────┐    ┌─────────────────────────┐
│ xrootd                   │    │ xrdmoncollect        │    │ Alloy ──► Prometheus    │
│  │ xrootd.monitor (UDP)  │    │  (collector mode)    │    │   :4318     :9090       │
│  ▼ localhost:9930        │    │                      │    │       ├───► Loki :3100  │
│ xrdmoncollect ───────────┼────┼─► :9931 ─────────────┼────┼─►     └───► Tempo       │
│  (shoveler mode)  TCP    │    │      OTLP/HTTPS      │    │                         │
│                 + token  │    │      + bearer token  │    │ Grafana :3000 ◄─queries─┤
└──────────────────────────┘    └──────────────────────┘    └─────────────────────────┘
```

- The **UDP hop stays on localhost** — UDP has no authentication and drops
  silently under loss; never send it across a WAN.
- The **shoveler** relays the raw datagrams over TCP (XSHV framing,
  preserving the original source addresses) with a shared-secret token, and
  spools to disk while the collector is unreachable.
- The **collector** decodes and correlates all streams and pushes OTLP
  (logs + traces) to Alloy over HTTPS with a bearer token, and/or posts
  documents directly to OpenSearch (section 17). Failed posts spool to disk.
- Aggregated metrics flow via Prometheus scrapes: the collector's own
  `/metrics` (`--metrics-port`) and each XRootD server's `/metrics`
  (XrdHttpMetricsExporter, bearer-token protected).

**Direct-UDP variant** (single host, or servers and collector on the same
switched LAN): point `xrootd.monitor ... dest ... <collector>:9930` straight
at the collector and skip the shovelers. Do **not** use this across routed
networks: monitoring datagrams (up to 64 KiB before tuning, section 2) get
IP-fragmented, one lost fragment discards the whole datagram, and the loss is
silent and unauthenticated. Whenever a network boundary is crossed, use a
shoveler.

The collector and backend may be co-located on one host, run at the site, or
run centrally (e.g. one collector per site pushing to a shared WLCG backend)
— the collector→backend hop is HTTPS with tokens and disk-buffered, so it
tolerates WAN outages.

### 0.2 Port map

| Component        | Port(s)                      | Role |
|------------------|------------------------------|------|
| xrootd           | 1094 (xroot), 8443 (https)   | data access; `/metrics` scrape endpoint on the http(s) port |
| xrdmoncollect (shoveler) | 9930/udp (localhost)  | receives `xrootd.monitor` datagrams |
| xrdmoncollect (collector) | 9930/udp, 9931/tcp   | 9930: direct UDP; 9931: shoveled streams (XSHV + token) |
| xrdmoncollect (both modes) | 9932               | Prometheus self-metrics (`--metrics-port`) |
| Alloy            | 4317 (OTLP gRPC), 4318 (OTLP HTTP), 12345 (UI) | telemetry ingest point |
| Prometheus       | 9090                         | metrics store (scrape + OTLP + remote-write) |
| Loki             | 3100                         | log store (default) |
| Tempo            | 3200 (API), 4319/4320 (OTLP) | trace store |
| Grafana          | 3000                         | dashboards + alerting |
| node_exporter    | 9100                         | host metrics (per machine) |
| OpenSearch       | 9200 (HTTPS)                 | log store (alternative) |
| OpenSearch Dashboards | 5601 (optional)         | OpenSearch-native UI |
| Data Prepper     | 21892 (OTLP logs in)         | OTLP → OpenSearch bridge |

Tempo's OTLP receiver deliberately uses **4319/4320** because Alloy owns
4317/4318 on the same host. This guide uses **9932** for the collector's
self-metrics so it never collides with the shovel TCP port (9931).

### 0.3 Bearer-token matrix

Every authenticated hop, the option that sends the token, and the option that
checks it:

| Hop | Sender | Receiver | Notes |
|-----|--------|----------|-------|
| xrootd → shoveler/collector (UDP) | — | — | no auth possible; keep on localhost (shoveler mode) |
| shoveler → collector (TCP) | `shovel-token = @<file>` | `tcp-token = @<file>` | constant-time compare; **without `tcp-token` the collector accepts any client** |
| collector → Alloy (OTLP) | `otlp-token = @<file>` | `otelcol.auth.bearer` (section 13) | plus TLS on the channel |
| collector → OpenSearch (direct) | `os-user`/`os-pass` or `os-token = @<file>` | OpenSearch security plugin | `os-token` needs JWT/bearer auth configured in OpenSearch; basic auth is the default setup |
| Prometheus → xrootd `/metrics` | `authorization: credentials:` in the scrape job | `metrics.authtoken` | XrdHttpMetricsExporter (section 2.3) |
| Prometheus → collector `:9932/metrics` | — | — | **no built-in auth** — restrict with the firewall / NetworkPolicy to the Prometheus host |

Conventions used throughout this guide:

- Tokens are random hex strings, generated once per hop and distributed to
  both ends:

  ```bash
  openssl rand -hex 32 | sudo tee /etc/xrootd/shovel.token > /dev/null
  openssl rand -hex 32 | sudo tee /etc/xrootd/otlp.token   > /dev/null
  openssl rand -hex 32 | sudo tee /etc/xrootd/metrics.token > /dev/null
  sudo chown xrootd:xrootd /etc/xrootd/*.token
  sudo chmod 600 /etc/xrootd/*.token
  ```

- xrdmoncollect token options use the `@<file>` form (`@/etc/xrootd/shovel.token`)
  so tokens never appear in `ps` output or world-readable configs.
- Three logical tokens: `shovel.token` (all shovelers ↔ site collector),
  `otlp.token` (collector ↔ Alloy — the same value goes into Alloy's
  `OTLP_BEARER_TOKEN`, section 13.2), `metrics.token` (Prometheus ↔ xrootd
  `/metrics`).

---

# Part I — XRootD servers and xrdmoncollect at the site

## 1. Topology: shoveler and collector modes

`xrdmoncollect` is one binary with two roles, selected by configuration:

- **Collector mode** (default): decodes the `xrootd.monitor` UDP streams,
  correlates opens/closes/transfers into complete OpenTelemetry-shaped JSON
  documents, and delivers them to sinks (OTLP → Alloy, OpenSearch `_bulk`,
  NDJSON file, TCP forward). It also serves aggregated Prometheus metrics.
- **Shoveler mode** (`shovel = <host:port>` set): does **not** decode.
  It receives the UDP datagrams locally and relays them over TCP to a central
  collector's `tcp-port`, framing each datagram with its original source
  address so the collector still knows which server sent it. While the
  collector is unreachable, datagrams spool to disk (`cache-dir`, capped by
  `spool-max`) and are replayed on reconnect.

Rules of thumb:

- **One shoveler per XRootD node, one collector per site.** The UDP hop
  stays on loopback; everything that crosses the network is TCP/HTTPS with
  tokens and disk buffering.
- **Direct UDP is acceptable** for a single-node site or when servers and
  collector share a switched LAN — after applying the buffer tuning in
  section 2.1.
- `shovel` and `tcp-port` are **mutually exclusive** — shovelers cannot be
  chained; they always talk directly to the collector.
- Run **one collector instance** per document stream: correlation state
  (open-file tables, dictionary maps) is per-process, so load-balancing one
  site's streams across multiple collectors would break correlation.
- In shoveler mode all decode/sink options are ignored with a warning, so a
  configuration-management system may push a single site-wide config file to
  shoveler nodes without harm; this guide still shows two separate files for
  clarity (section 3).

## 2. XRootD server configuration

The directives below go into the xrootd config file on **every data server**
(and are identical in all four deployment tracks).

### 2.1 Detailed monitoring stream

```
all.sitename EXAMPLE-SITE

xrootd.monitor all auth flush io 60s fstat 60s lfn ops ssq xfr 10 \
               mbuff 1472 fbsz 1472 rbuff 1472 gbuff 1472 window 15s \
               dest files fstat io info redir user localhost:9930
```

What matters and why:

- `all.sitename` is carried in the identity records and becomes the
  `wlcg.site` attribute on every document — set it to your WLCG site name.
- `lfn` — adds the file path to open events; without it documents have no
  file names.
- `xfr 10` — emits in-progress transfer snapshots; **required** for the
  collector to report bytes for long-running transfers and to notice
  transfers that never close cleanly.
- `auth` — enriches the user stream with the authentication method and
  VO/role, which feed the `wlcg.vo` and auth attributes.
- `ops ssq` — operation counts and sum-of-squares statistics on close.
- **Buffer sizes**: every buffer becomes one UDP datagram. A datagram larger
  than the path MTU is IP-fragmented and **one lost fragment discards the
  whole datagram** — silently. `1472` fits a 1500-byte MTU for IPv4+UDP
  (use `1452` for IPv6). This matters even on localhost once a shoveler
  relays the datagrams onward, because oversized datagrams also stress the
  receive path.
- **`fbsz` must be set explicitly on servers older than XRootD 6.1**: the
  fstat buffer did not follow `mbuff` and defaulted to 65472 bytes — 44 IP
  fragments per datagram on a 1500-MTU path. The fstat stream is what
  transfer documents are built from, so this is the single most important
  tuning knob. From 6.1 on, `fbsz` defaults to the `mbuff` value.
- `dest ... localhost:9930` — with a shoveler on the node (recommended).
  For the direct-UDP variant, use the collector host instead.

With shovelers deployed, servers with different MTUs or older versions still
work — the collector tracks per-server, per-stream sequence gaps
(`xrootd_collector_packets_lost_total`), so misconfigured servers are visible
in the metrics rather than silently absent (section 20).

### 2.2 Summary stream (`xrd.report`) — do not point it at the collector

`xrdmoncollect` decodes the *detailed* monitoring streams only. The classic
`xrd.report` summary packets are a different (XML) format; if you send them
to the collector's port they count as `unknown_packets_total` and are
dropped. Aggregated numbers come instead from the collector's own
`/metrics` endpoint and from the native metrics exporter below. Keep
`xrd.report` only if some other consumer (e.g. an existing MonALISA/legacy
pipeline) still needs it — pointed at that consumer, not at xrdmoncollect.

### 2.3 Native Prometheus metrics (XrdHttpMetricsExporter)

Server-level metrics (connections, threads, buffers, requests, HTTP, OFS…)
are exported natively by the metrics exporter plugin, scraped by Prometheus
with a bearer token:

```
# HTTP protocol stack (skip if xrd.protocol http is already configured).
xrd.protocol http:8443 libXrdHttp.so

# Serve Prometheus text format on /metrics. Drop +notls when the http port
# has TLS configured (recommended for production).
http.exthandler metrics +notls libXrdHttpMetricsExporter.so

# Require a bearer token for scrapes (Authorization: Bearer <token>).
metrics.authtoken <contents of /etc/xrootd/metrics.token>
metrics.label site EXAMPLE-SITE
```

The matching Prometheus scrape job (added in section 9):

```yaml
  - job_name: xrootd
    metrics_path: /metrics
    authorization:
      credentials: <contents of /etc/xrootd/metrics.token>
    static_configs:
      - targets: ["<xrootd-host>:8443"]
```

The exporter can also **push** OTLP metrics instead of being scraped
(`metrics.otelurl https://<alloy-host>:4318/v1/metrics`,
`metrics.otelinterval 30`) — useful when inbound scraping is impossible.

## 3. xrdmoncollect configuration

Configuration lives in `/etc/xrootd/xrdmoncollect.cfg` (INI format, a single
`[xrdmoncollect]` section; keys mirror the long command-line options, and
command-line options override the file). The two files below are used
verbatim by every deployment track — only hostnames change.

### 3.1 Shoveler nodes

```ini
# /etc/xrootd/xrdmoncollect.cfg — every XRootD node (shoveler mode)
[xrdmoncollect]

# Receive xrootd.monitor datagrams from the local server.
port = 9930

# Relay them over TCP to the site collector, authenticated by shared secret.
shovel = collector.example.org:9931
shovel-token = @/etc/xrootd/shovel.token

# Spool datagrams here while the collector is unreachable (oldest evicted
# beyond spool-max). Under systemd, /var/lib/xrootd is the StateDirectory.
cache-dir = /var/lib/xrootd/moncollect
spool-max = 1G

# Kernel UDP receive buffer.
rcvbuf = 16M

# Prometheus self-metrics (shoveler pipeline + spool health).
metrics-port = 9932
```

### 3.2 Central collector

```ini
# /etc/xrootd/xrdmoncollect.cfg — site collector host (collector mode)
[xrdmoncollect]

# Direct UDP from same-LAN servers (optional if all nodes run shovelers).
port = 9930

# Shoveled streams over TCP, token-authenticated.
tcp-port = 9931
tcp-token = @/etc/xrootd/shovel.token

# Primary sink: OTLP to Grafana Alloy (section 13). Logs go to /v1/logs;
# with spans enabled, file-operation spans go to /v1/traces as well.
otlp-url = https://alloy.example.org:4318
otlp-token = @/etc/xrootd/otlp.token
spans = true

# Alternative or additional sink: post documents directly to OpenSearch
# (section 17) — no Data Prepper needed on this path.
# os-url = https://opensearch.example.org:9200
# os-index = xrootd-transfers
# os-datastream = true
# os-user = collector
# os-pass = <password>            # or: os-token = @/etc/xrootd/opensearch.token

# On-failure disk cache: bodies that fail to POST are spooled here and
# replayed oldest-first (survives collector restarts).
cache-dir = /var/lib/xrootd/moncollect

# Prometheus self-metrics: decoder health, packet loss, sink health,
# per-VO/locality transfer aggregates.
metrics-port = 9932

# Correlation-state persistence across clean restarts (defaults into the
# systemd StateDirectory; shown here for the container tracks).
state-file = /var/lib/xrootd/xrdmoncollect-state.json
state-ttl = 15m

# --- Tuning (defaults shown; see the capacity table in the README) -------
# flush-count = 500        # packets per batch (one batch -> one POST)
# flush-secs = 5           # max age of a partial batch
# rcvbuf = 16M             # kernel UDP receive buffer
# queue-depth = 64         # receiver -> serializer batches in flight
# max-memory = 256M        # correlation-state budget (LRU eviction above)
# server-ttl = 86400       # reap idle server incarnations after this many s

# --- Enrichment (optional) ------------------------------------------------
# First capture group becomes the xrootd.dataset attribute (feeds the
# data-popularity dashboards). Example for a CMS-style /store namespace:
# dataset = ^/store/[^/]+/[^/]+/([^/]+)/
# SciTags registry for experiment/activity names on flow-tagged transfers:
# scitags = https://www.scitags.org/api.json
# scitags-refresh = 3600

# --- Optional document streams (higher volume, off by default) ------------
# sessions = false         # per-session rollup on client disconnect
# traces = false           # raw I/O trace documents
# gstream = false          # plugin (pfc/tpc/tcpmon...) records
# redirects = false        # redirect events
```

TLS note: `otlp-url` is HTTPS. If Alloy uses the self-signed certificate
from section 13.1, add it to the collector host's trust store rather than
disabling verification:

```bash
sudo cp alloy-server.crt /etc/pki/ca-trust/source/anchors/
sudo update-ca-trust
```

(`otlp-insecure = true` / `os-insecure = true` skip verification — lab use
only.)

## 4. Track A — native packages (VMs / bare metal)

### 4.1 Installing XRootD ≥ 6.1

`xrdmoncollect` and its systemd units ship in the `xrootd-server` package of
XRootD 6.1+ (the metrics exporter plugin comes with its `xrootd-server-libs`
dependency). Once released, install from EPEL or
from the XRootD upstream repository:

```bash
sudo dnf -y install epel-release
sudo dnf -y install xrootd-server        # >= 6.1
```

Until 6.1 packages are published (or to deploy a pre-release), build the
RPMs from a source checkout on a build host of the same OS:

```bash
sudo dnf -y install git rpmdevtools dnf-plugins-core epel-release
sudo dnf config-manager --set-enabled crb
git clone https://github.com/xrootd/xrootd.git && cd xrootd
sudo dnf -y builddep xrootd.spec
rpmdev-setuptree
git archive --prefix=xrootd/ -o ~/rpmbuild/SOURCES/xrootd.tar.gz HEAD
rpmbuild -bb --with git xrootd.spec
# install on the target nodes:
sudo dnf -y install ~/rpmbuild/RPMS/*/xrootd-server-*.rpm \
                    ~/rpmbuild/RPMS/*/xrootd-libs-*.rpm \
                    ~/rpmbuild/RPMS/*/xrootd-client-libs-*.rpm \
                    ~/rpmbuild/RPMS/*/xrootd-server-libs-*.rpm
```

The package installs `/usr/bin/xrdmoncollect`, the commented example config
as `/etc/xrootd/xrdmoncollect.cfg`, the `xrdmoncollect.service` +
`xrdmoncollect.socket` units, and `man 8 xrdmoncollect`.

### 4.2 Shoveler nodes (every XRootD server)

1. Add the section 2 directives to the xrootd config and restart xrootd.
2. Install `/etc/xrootd/shovel.token` (section 0.3) and replace
   `/etc/xrootd/xrdmoncollect.cfg` with the shoveler config from 3.1.
3. Enable socket + service:

```bash
sudo systemctl enable --now xrdmoncollect.socket
sudo systemctl enable --now xrdmoncollect.service
```

**Why the socket unit matters:** systemd owns the UDP socket and passes it
to the daemon (socket activation). Because the socket stays open across
service restarts, datagrams queue in the kernel receive buffer
(`ReceiveBuffer=16M` in the unit) instead of being lost — restarts and even
crash-restarts drop nothing that fits in the buffer. The port in the socket
unit must match `port` in the config; on mismatch the daemon binds its own
socket and only the zero-loss-restart property is lost.

No firewall changes are needed: UDP 9930 stays on loopback and the shoveler
makes an *outbound* TCP connection to the collector.

### 4.3 Central collector host

1. Install `xrootd-server` the same way; no xrootd configuration is needed
   on a dedicated collector host.
2. Install `/etc/xrootd/shovel.token` and `/etc/xrootd/otlp.token`, and the
   collector config from 3.2.
3. The packaged socket unit only listens on UDP; add the TCP listener via a
   drop-in so it also survives restarts:

```bash
sudo systemctl edit xrdmoncollect.socket
```

```ini
[Socket]
ListenStream=9931
```

4. Enable and open the firewall — shovel TCP from the site's server subnet,
   self-metrics to the Prometheus host only:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now xrdmoncollect.socket xrdmoncollect.service

sudo firewall-cmd --permanent \
  --add-rich-rule='rule family="ipv4" source address="<SERVER_SUBNET_CIDR>" port port="9931" protocol="tcp" accept'
sudo firewall-cmd --permanent \
  --add-rich-rule='rule family="ipv4" source address="<PROM_IP>" port port="9932" protocol="tcp" accept'
sudo firewall-cmd --reload
```

### 4.4 Verify

```bash
journalctl -u xrdmoncollect -f                  # startup, sink and shovel logs
curl -s localhost:9932/metrics | head           # self-metrics up
ss -ulpn 'sport = 9930'; ss -tlpn 'sport = 9931'  # sockets owned by systemd
```

On a shoveler node, the `xrootd_shoveler_*` frame and spool metrics tell you
whether frames flow, spool, or drop; on the collector,
`xrootd_collector_packets_lost_total` and `malformed_total` expose per-server
stream health (details in section 20).

## 5. Track B — podman

### 5.1 Build the image locally

No published container image carries the 6.1 monitoring features yet, so the
image is built from a source checkout. The multi-stage Containerfile below
builds the RPMs in a throwaway stage (the same recipe as the repo's
`docker/Dockerfile.alma9`) and installs only the runtime packages:

```dockerfile
# Containerfile.mon — XRootD server + xrdmoncollect from source RPMs
FROM almalinux:9 AS build

RUN dnf install -y dnf-plugins-core epel-release rpmdevtools \
 && dnf config-manager --set-enabled crb

WORKDIR /root
RUN rpmdev-setuptree

# Create the tarball first: git archive --prefix=xrootd/ -o xrootd.tar.gz HEAD
COPY xrootd.tar.gz rpmbuild/SOURCES/
RUN tar xzf rpmbuild/SOURCES/xrootd.tar.gz --strip-components=1 xrootd/xrootd.spec \
 && dnf builddep -y xrootd.spec \
 && rpmbuild -bb --with git xrootd.spec

FROM almalinux:9

RUN --mount=type=bind,from=build,source=/root/rpmbuild/RPMS,target=/rpms \
    dnf install -y epel-release \
 && dnf install -y $(find /rpms -name '*.rpm' \
        ! -name '*-debuginfo-*' ! -name '*-debugsource-*' \
        ! -name '*-devel-*'     ! -name '*-tests*') \
 && dnf clean all

# The xrootd-server RPM creates the xrootd user and the config/log/state
# directory layout, and installs the example /etc/xrootd/xrdmoncollect.cfg.
USER xrootd
```

Build from the repository root:

```bash
git clone https://github.com/xrootd/xrootd.git && cd xrootd
git archive --prefix=xrootd/ -o xrootd.tar.gz HEAD
podman build -t localhost/xrootd-mon:6.1 -f Containerfile.mon .
```

> The RPM build compiles the whole project — expect it to take a while.
> `docker/xrd-docker build alma9` produces an equivalent (bigger,
> build-tools-included) image tagged `xrootd:alma9` if you prefer the
> repo-maintained recipe.

If the collector will talk to an Alloy with a self-signed certificate, bake
the CA into the image (mounted trust anchors are not picked up
automatically):

```dockerfile
# append to the runtime stage, before USER xrootd:
COPY alloy-server.crt /etc/pki/ca-trust/source/anchors/
RUN update-ca-trust
```

### 5.2 Run the containers

Uses the configs and tokens from sections 3 and 0.3 unchanged, bind-mounted
into the container (`:Z` relabels for SELinux). The spool/state area is a
named volume so it survives container replacement.

**Shoveler on an XRootD node** — must share the host's loopback so it can
receive the server's UDP datagrams, hence `--network host`:

```bash
sudo podman run -d --name moncollect-shoveler --network host \
  -v /etc/xrootd/xrdmoncollect.cfg:/etc/xrootd/xrdmoncollect.cfg:ro,Z \
  -v /etc/xrootd/shovel.token:/etc/xrootd/shovel.token:ro,Z \
  -v moncollect-spool:/var/lib/xrootd \
  localhost/xrootd-mon:6.1 xrdmoncollect -c /etc/xrootd/xrdmoncollect.cfg
```

**Central collector:**

```bash
sudo podman run -d --name moncollect-collector \
  -p 9930:9930/udp -p 9931:9931 -p 9932:9932 \
  -v /etc/xrootd/xrdmoncollect.cfg:/etc/xrootd/xrdmoncollect.cfg:ro,Z \
  -v /etc/xrootd/shovel.token:/etc/xrootd/shovel.token:ro,Z \
  -v /etc/xrootd/otlp.token:/etc/xrootd/otlp.token:ro,Z \
  -v moncollect-state:/var/lib/xrootd \
  localhost/xrootd-mon:6.1 xrdmoncollect -c /etc/xrootd/xrdmoncollect.cfg
```

Alternative to bind-mounted token files: podman secrets
(`podman secret create shovel-token /etc/xrootd/shovel.token`, run with
`--secret shovel-token`, and point the config at
`@/run/secrets/shovel-token`).

### 5.3 Run under systemd (Quadlet)

On AlmaLinux 9/10, Quadlet is the native way to keep podman containers
running. `/etc/containers/systemd/moncollect.container`:

```ini
[Unit]
Description=XRootD monitoring collector (container)
After=network-online.target
Wants=network-online.target

[Container]
Image=localhost/xrootd-mon:6.1
Exec=xrdmoncollect -c /etc/xrootd/xrdmoncollect.cfg
PublishPort=9930:9930/udp
PublishPort=9931:9931
PublishPort=9932:9932
Volume=/etc/xrootd/xrdmoncollect.cfg:/etc/xrootd/xrdmoncollect.cfg:ro,Z
Volume=/etc/xrootd/shovel.token:/etc/xrootd/shovel.token:ro,Z
Volume=/etc/xrootd/otlp.token:/etc/xrootd/otlp.token:ro,Z
Volume=moncollect-state:/var/lib/xrootd

[Service]
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl start moncollect
```

For a shoveler node, replace the `PublishPort=` lines with `Network=host`
and drop the `otlp.token` volume.

> **Restart coverage:** systemd **socket activation is not available inside
> containers**, so a collector restart briefly closes the UDP/TCP ports.
> The shovelers bridge exactly this gap — they spool while the collector is
> down and replay on reconnect. In the direct-UDP variant, datagrams sent
> during a container restart are lost; prefer Track A (native packages, with
> the socket unit) for a collector that receives direct UDP.

## 6. Track C — docker compose (full stack on one host)

An all-in-one recipe — XRootD, shoveler, collector *and* the Part II backend
— suitable for evaluation, small sites, or as the starting point for a
split deployment (in production, run the `xrootd` + `shoveler` services at
the site and the rest on the monitoring host, changing only the hostnames
and enabling TLS on Alloy as in section 13).

Directory layout:

```
monitoring/
├── compose.yaml
├── Containerfile.mon              # from section 5.1 (plus xrootd.tar.gz)
├── xrootd.tar.gz
├── secrets/
│   ├── shovel.token               # openssl rand -hex 32 > ...
│   └── otlp.token
├── configs/
│   ├── xrootd.cfg                 # section 2 directives + a data export
│   ├── shoveler.cfg               # section 3.1, shovel = collector:9931
│   ├── collector.cfg              # section 3.2, otlp-url = http://alloy:4318
│   ├── config.alloy               # section 13.3 with the deltas below
│   ├── prometheus.yml             # section 9 with the deltas below
│   ├── loki.yml                   # section 11, unchanged
│   ├── tempo.yml                  # section 12, unchanged
│   ├── grafana-datasources.yaml   # section 14, localhost -> service names
│   └── grafana-dashboards.yaml    # dashboard provider (below)
└── dashboards/                    # the three grafana-*.json from this dir
```

`compose.yaml`:

```yaml
name: xrootd-monitoring

secrets:
  shovel-token: { file: ./secrets/shovel.token }
  otlp-token:   { file: ./secrets/otlp.token }

volumes:
  xrootd-data:
  shoveler-spool:
  collector-state:
  prometheus-data:
  loki-data:
  tempo-data:
  grafana-data:
  opensearch-data:

services:
  xrootd:
    build: { context: ., dockerfile: Containerfile.mon }
    image: localhost/xrootd-mon:6.1
    command: xrootd -c /etc/xrootd/xrootd.cfg
    volumes:
      - ./configs/xrootd.cfg:/etc/xrootd/xrootd.cfg:ro,Z
      - xrootd-data:/data
    ports: ["1094:1094", "8443:8443"]

  shoveler:
    image: localhost/xrootd-mon:6.1
    command: xrdmoncollect -c /etc/xrootd/xrdmoncollect.cfg
    network_mode: "service:xrootd"     # shares loopback with xrootd
    volumes:
      - ./configs/shoveler.cfg:/etc/xrootd/xrdmoncollect.cfg:ro,Z
      - shoveler-spool:/var/lib/xrootd
    secrets: [shovel-token]
    depends_on: [xrootd]

  collector:
    image: localhost/xrootd-mon:6.1
    command: xrdmoncollect -c /etc/xrootd/xrdmoncollect.cfg
    volumes:
      - ./configs/collector.cfg:/etc/xrootd/xrdmoncollect.cfg:ro,Z
      - collector-state:/var/lib/xrootd
    secrets: [shovel-token, otlp-token]
    expose: ["9931", "9932"]
    healthcheck:
      test: ["CMD", "curl", "-sf", "http://localhost:9932/metrics"]
      interval: 30s

  alloy:
    image: grafana/alloy:latest
    command: ["run", "--server.http.listen-addr=0.0.0.0:12345", "/etc/alloy/config.alloy"]
    volumes:
      - ./configs/config.alloy:/etc/alloy/config.alloy:ro,Z
    secrets: [otlp-token]
    expose: ["4317", "4318"]
    ports: ["12345:12345"]             # Alloy UI (optional)

  prometheus:
    image: prom/prometheus:latest
    command:
      - --config.file=/etc/prometheus/prometheus.yml
      - --storage.tsdb.retention.time=90d
      - --web.enable-otlp-receiver
      - --web.enable-remote-write-receiver
      - --web.enable-lifecycle
    volumes:
      - ./configs/prometheus.yml:/etc/prometheus/prometheus.yml:ro,Z
      - prometheus-data:/prometheus
    ports: ["9090:9090"]

  loki:
    image: grafana/loki:latest
    command: ["-config.file=/etc/loki/config.yml"]
    volumes:
      - ./configs/loki.yml:/etc/loki/config.yml:ro,Z
      - loki-data:/var/lib/loki
    expose: ["3100"]

  tempo:
    image: grafana/tempo:latest
    command: ["-config.file=/etc/tempo/config.yml"]
    volumes:
      - ./configs/tempo.yml:/etc/tempo/config.yml:ro,Z
      - tempo-data:/var/lib/tempo
    expose: ["3200", "4319"]

  grafana:
    image: grafana/grafana:latest
    volumes:
      - grafana-data:/var/lib/grafana
      - ./configs/grafana-datasources.yaml:/etc/grafana/provisioning/datasources/datasources.yaml:ro,Z
      - ./configs/grafana-dashboards.yaml:/etc/grafana/provisioning/dashboards/xrootd.yaml:ro,Z
      - ./dashboards:/etc/grafana/dashboards:ro,Z
    ports: ["3000:3000"]

  # --- optional OpenSearch log backend: docker compose --profile opensearch up
  opensearch:
    profiles: [opensearch]
    image: opensearchproject/opensearch:3
    environment:
      - discovery.type=single-node
      - OPENSEARCH_INITIAL_ADMIN_PASSWORD=<StrongAdminPassword>
      - OPENSEARCH_JAVA_OPTS=-Xms2g -Xmx2g
    ulimits:
      memlock: { soft: -1, hard: -1 }
    volumes:
      - opensearch-data:/usr/share/opensearch/data
    ports: ["9200:9200"]

  data-prepper:
    profiles: [opensearch]
    image: opensearchproject/data-prepper:latest
    volumes:
      - ./configs/data-prepper-pipelines.yaml:/usr/share/data-prepper/pipelines/pipelines.yaml:ro,Z
    expose: ["21892"]
    depends_on: [opensearch]
```

Config deltas versus the native Part II files — inside the compose network,
`localhost` becomes the service name, and the Alloy OTLP hop drops TLS (a
private bridge network on one host) while keeping the bearer token:

| File | Change |
|------|--------|
| `shoveler.cfg` | `shovel = collector:9931`; tokens at `@/run/secrets/shovel-token` |
| `collector.cfg` | `otlp-url = http://alloy:4318`; tokens at `@/run/secrets/...` |
| `prometheus.yml` | scrape targets `alloy:12345`, `collector:9932`, `xrootd:8443` (job from 2.3) |
| `config.alloy` | remove the `tls { ... }` blocks from the receiver; exporters point at `prometheus:9090`, `loki:3100`, `tempo:4319`; token from the secret file (below) |
| `grafana-datasources.yaml` | `http://prometheus:9090`, `http://loki:3100`, `http://tempo:3200` |
| `tempo.yml` | `remote_write` url `http://prometheus:9090/api/v1/write` |

Alloy reads the token from the compose secret instead of an environment
file — replace the `otelcol.auth.bearer` block of section 13.3 with:

```alloy
local.file "otlp_token" {
  filename  = "/run/secrets/otlp-token"
  is_secret = true
}

otelcol.auth.bearer "otlp_in" {
  token = local.file.otlp_token.content
}
```

`configs/grafana-dashboards.yaml` provisions the XRootD dashboards shipped
next to this guide (copy `grafana-dashboard.json`,
`grafana-loki-dashboard.json` and `grafana-loki-popularity-dashboard.json`
into `dashboards/`):

```yaml
apiVersion: 1
providers:
  - name: xrootd
    folder: XRootD
    type: file
    options:
      path: /etc/grafana/dashboards
```

Bring it up and smoke-test:

```bash
docker compose up -d --build          # add --profile opensearch for OpenSearch
docker compose ps                     # collector healthcheck: healthy
xrdcp /etc/hostname xroot://localhost:1094//data/smoke-test
```

Then follow section 20 (verification) — the transfer document appears in
Grafana → Explore → Loki within one flush interval.

## 7. Track D — Kubernetes

This track runs the **central collector** in a cluster; shovelers run where
the XRootD servers are (natively per Track A, or as a sidecar when xrootd
itself is a Pod). Push the locally-built image to a registry the cluster can
reach:

```bash
podman tag localhost/xrootd-mon:6.1 registry.example.org/xrootd/xrootd-mon:6.1
podman push registry.example.org/xrootd/xrootd-mon:6.1
```

### 7.1 Namespace, tokens, config

```yaml
apiVersion: v1
kind: Namespace
metadata:
  name: xrootd-monitoring
```

```bash
kubectl -n xrootd-monitoring create secret generic moncollect-tokens \
  --from-file=shovel.token=/etc/xrootd/shovel.token \
  --from-file=otlp.token=/etc/xrootd/otlp.token
```

The collector config is the section 3.2 file with the token paths pointing
into the secret mount:

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: moncollect-config
  namespace: xrootd-monitoring
data:
  xrdmoncollect.cfg: |
    [xrdmoncollect]
    tcp-port = 9931
    tcp-token = @/etc/xrootd/tokens/shovel.token
    otlp-url = https://alloy.example.org:4318
    otlp-token = @/etc/xrootd/tokens/otlp.token
    spans = true
    cache-dir = /var/lib/xrootd/moncollect
    state-file = /var/lib/xrootd/xrdmoncollect-state.json
    metrics-port = 9932
```

(No `port =` — a TCP-only collector; UDP senders can't reach a ClusterIP
sensibly anyway. If the backend also runs in-cluster, point `otlp-url` at
its service, e.g. `http://alloy.monitoring.svc:4318`.)

### 7.2 Collector Deployment, PVC, Service

```yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: moncollect-state
  namespace: xrootd-monitoring
spec:
  accessModes: [ReadWriteOnce]
  resources:
    requests:
      storage: 10Gi              # spool + correlation state; see section 18
  # storageClassName: <site default>
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: moncollect-collector
  namespace: xrootd-monitoring
spec:
  replicas: 1        # correlation state is per-process — never scale this out
  strategy:
    type: Recreate   # single writer on the PVC
  selector:
    matchLabels:
      app: moncollect-collector
  template:
    metadata:
      labels:
        app: moncollect-collector
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9932"
    spec:
      containers:
        - name: collector
          image: registry.example.org/xrootd/xrootd-mon:6.1
          command: ["xrdmoncollect", "-c", "/etc/xrootd/xrdmoncollect.cfg"]
          ports:
            - name: shovel
              containerPort: 9931
            - name: metrics
              containerPort: 9932
          readinessProbe:
            httpGet:
              path: /metrics
              port: metrics
            initialDelaySeconds: 5
          livenessProbe:
            httpGet:
              path: /metrics
              port: metrics
            periodSeconds: 30
          resources:
            requests: { cpu: 500m, memory: 512Mi }
            limits: { memory: 1Gi }     # keep above max-memory + headroom
          volumeMounts:
            - name: config
              mountPath: /etc/xrootd/xrdmoncollect.cfg
              subPath: xrdmoncollect.cfg
            - name: tokens
              mountPath: /etc/xrootd/tokens
              readOnly: true
            - name: state
              mountPath: /var/lib/xrootd
      volumes:
        - name: config
          configMap:
            name: moncollect-config
        - name: tokens
          secret:
            secretName: moncollect-tokens
            defaultMode: 0400
        - name: state
          persistentVolumeClaim:
            claimName: moncollect-state
---
apiVersion: v1
kind: Service
metadata:
  name: moncollect
  namespace: xrootd-monitoring
spec:
  selector:
    app: moncollect-collector
  ports:
    - name: shovel
      port: 9931
      targetPort: shovel
    - name: metrics
      port: 9932
      targetPort: metrics
```

Shovelers **outside** the cluster need a stable, routable entry point for
port 9931 — add a dedicated exposed Service (keep `metrics` internal):

```yaml
apiVersion: v1
kind: Service
metadata:
  name: moncollect-shovel-ingest
  namespace: xrootd-monitoring
spec:
  type: LoadBalancer               # or NodePort on bare-metal clusters
  selector:
    app: moncollect-collector
  ports:
    - name: shovel
      port: 9931
      targetPort: shovel
```

Point the shoveler nodes' `shovel =` at the load-balancer address. The XSHV
hello is token-checked (constant-time) and malformed frames drop the
connection, so exposing 9931 is safe; a `NetworkPolicy` still narrows it:

```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: moncollect-ingress
  namespace: xrootd-monitoring
spec:
  podSelector:
    matchLabels:
      app: moncollect-collector
  ingress:
    - from:
        - ipBlock:
            cidr: <SERVER_SUBNET_CIDR>       # site's XRootD nodes
      ports:
        - port: 9931
    - from:
        - namespaceSelector:
            matchLabels:
              kubernetes.io/metadata.name: monitoring   # Prometheus
      ports:
        - port: 9932
```

### 7.3 Shoveler patterns

**(a) XRootD on the cluster nodes themselves** — run the shoveler as a
DaemonSet in the host network namespace, so the node-local xrootd reaches it
at `localhost:9930`:

```yaml
apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: moncollect-shoveler
  namespace: xrootd-monitoring
spec:
  selector:
    matchLabels:
      app: moncollect-shoveler
  template:
    metadata:
      labels:
        app: moncollect-shoveler
    spec:
      hostNetwork: true
      # nodeSelector: { xrootd: "true" }   # only where xrootd runs
      containers:
        - name: shoveler
          image: registry.example.org/xrootd/xrootd-mon:6.1
          command: ["xrdmoncollect", "-c", "/etc/xrootd/xrdmoncollect.cfg"]
          volumeMounts:
            - name: config
              mountPath: /etc/xrootd/xrdmoncollect.cfg
              subPath: xrdmoncollect.cfg
            - name: tokens
              mountPath: /etc/xrootd/tokens
              readOnly: true
            - name: spool
              mountPath: /var/lib/xrootd
      volumes:
        - name: config
          configMap:
            name: shoveler-config     # section 3.1 cfg, shovel = moncollect.xrootd-monitoring.svc:9931
        - name: tokens
          secret:
            secretName: moncollect-tokens
            defaultMode: 0400
        - name: spool
          hostPath:
            path: /var/lib/xrootd-shoveler
            type: DirectoryOrCreate
```

**(b) XRootD itself runs as Pods** — add the shoveler as a second container
in the xrootd Pod; containers in a Pod share the loopback interface, so the
UDP hop never leaves the Pod:

```yaml
# excerpt: containers of the xrootd Pod template
containers:
  - name: xrootd
    image: registry.example.org/xrootd/xrootd-mon:6.1
    command: ["xrootd", "-c", "/etc/xrootd/xrootd.cfg"]
    # xrootd.monitor ... dest ... localhost:9930 (section 2)
  - name: shoveler
    image: registry.example.org/xrootd/xrootd-mon:6.1
    command: ["xrdmoncollect", "-c", "/etc/xrootd/xrdmoncollect.cfg"]
    volumeMounts:
      - name: shoveler-config
        mountPath: /etc/xrootd/xrdmoncollect.cfg
        subPath: xrdmoncollect.cfg
      - name: tokens
        mountPath: /etc/xrootd/tokens
        readOnly: true
      - name: spool
        mountPath: /var/lib/xrootd    # emptyDir (spool lost with the Pod) or PVC
```

### 7.4 Prometheus scraping and the backend

With the Prometheus Operator, replace the pod annotations by a
ServiceMonitor:

```yaml
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: moncollect
  namespace: xrootd-monitoring
spec:
  selector:
    matchLabels:
      app: moncollect-collector
  endpoints:
    - port: metrics
      interval: 30s
```

The backend itself is deployed with the upstream Helm charts rather than
hand-written manifests — `kube-prometheus-stack` (Prometheus + Grafana),
`grafana/loki`, `grafana/tempo`, `grafana/alloy` (and `opensearch` when
using the section 17 path). Apply the same configuration choices as
Part II: Alloy's OTLP receiver with TLS + `otelcol.auth.bearer`, Prometheus
with the OTLP receiver enabled, Loki with structured metadata. Storage for
all of them: section 18.

---

# Part II — The central monitoring backend (AlmaLinux 10, native packages)

**Target:** a single AlmaLinux 10 node running Alloy, Prometheus, Loki, Tempo
and Grafana as native systemd services (OpenSearch as the alternative log
backend in section 17). For a containerized backend, reuse these exact
configuration files with the section 6 compose stack or the section 7.4 Helm
charts.

## 8. System prep

```bash
sudo dnf -y update
sudo dnf -y install wget curl tar policycoreutils-python-utils epel-release
getenforce    # AlmaLinux 10 ships Enforcing; keep it. Check `ausearch -m avc` on failures.
```

---

## 9. Prometheus (from EPEL)

```bash
sudo dnf -y install prometheus
```

The package provides the service user, `/etc/prometheus/prometheus.yml`, the
data directory, and a systemd unit. Three non-default flags are required:

| Flag | Why |
|------|-----|
| `--web.enable-otlp-receiver`         | accept OTLP metrics pushed by Alloy |
| `--web.enable-remote-write-receiver` | accept remote-write (Alloy node metrics, Tempo span metrics) |
| `--web.enable-lifecycle`             | allow `POST /-/reload` for config reloads |

Find where the packaged unit takes its arguments and add the flags there:

```bash
systemctl cat prometheus     # look for EnvironmentFile= / an $OPTIONS-style variable
```

If the unit reads an environment file (typically `/etc/sysconfig/prometheus`),
append the flags to its args variable. Otherwise, use a systemd drop-in:

```bash
sudo systemctl edit prometheus
```

```ini
[Service]
ExecStart=
ExecStart=<ExecStart line copied from `systemctl cat prometheus`> \
  --web.enable-otlp-receiver \
  --web.enable-remote-write-receiver \
  --web.enable-lifecycle
```

Configuration (`/etc/prometheus/prometheus.yml`):

```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

# Tolerate slightly out-of-order samples from OTLP/remote-write batching.
storage:
  tsdb:
    out_of_order_time_window: 30m

# OTLP ingestion behavior (native /api/v1/otlp endpoint).
otlp:
  # Promote a small set of OTel resource attributes to metric labels.
  promote_resource_attributes:
    - service.name
    - service.namespace
    - service.instance.id
    - deployment.environment.name

scrape_configs:
  - job_name: prometheus
    static_configs:
      - targets: ["localhost:9090"]

  - job_name: alloy
    static_configs:
      - targets: ["localhost:12345"]

  - job_name: node
    static_configs:
      - targets:
          - "localhost:9100"       # local node_exporter
          # - "<REMOTE_IP>:9100"   # add remote machines here (see section 16)

  # xrdmoncollect self-metrics: decoder/sink health, packet loss,
  # per-VO/locality transfer aggregates (collector and shoveler nodes alike).
  - job_name: moncollect
    static_configs:
      - targets:
          - "<COLLECTOR_HOST>:9932"
          # - "<XROOTD_NODE>:9932"   # shovelers, if their 9932 is opened

  # XRootD native metrics (XrdHttpMetricsExporter, section 2.3),
  # bearer-token protected.
  - job_name: xrootd
    metrics_path: /metrics
    authorization:
      credentials: <contents of /etc/xrootd/metrics.token>
    static_configs:
      - targets:
          - "<XROOTD_HOST>:8443"
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now prometheus
curl -s localhost:9090/-/ready
```

> Reload config after edits without restarting:
> `curl -X POST http://localhost:9090/-/reload`

---

## 10. Grafana repo + package install

One repository provides Grafana, Loki, Tempo, and Alloy:

```bash
sudo wget -q -O /etc/pki/rpm-gpg/grafana.key https://rpm.grafana.com/gpg.key
sudo rpm --import /etc/pki/rpm-gpg/grafana.key

sudo tee /etc/yum.repos.d/grafana.repo > /dev/null <<'EOF'
[grafana]
name=grafana
baseurl=https://rpm.grafana.com
repo_gpgcheck=1
enabled=1
gpgcheck=1
gpgkey=https://rpm.grafana.com/gpg.key
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
EOF

sudo dnf -y install grafana loki tempo alloy
```

Before copying configs below, confirm the config path each packaged unit
expects (they occasionally differ between package revisions):

```bash
systemctl cat loki  | grep -i config
systemctl cat tempo | grep -i config
```

---

## 11. Loki (default log store)

`/etc/loki/config.yml` — monolithic mode, filesystem storage, OTLP-ready:

```yaml
auth_enabled: false

server:
  http_listen_port: 3100
  grpc_listen_port: 9096
  log_level: info

common:
  instance_addr: 127.0.0.1
  path_prefix: /var/lib/loki
  storage:
    filesystem:
      chunks_directory: /var/lib/loki/chunks
      rules_directory: /var/lib/loki/rules
  replication_factor: 1
  ring:
    kvstore:
      store: inmemory

schema_config:
  configs:
    - from: 2024-01-01
      store: tsdb
      object_store: filesystem
      schema: v13
      index:
        prefix: index_
        period: 24h

limits_config:
  allow_structured_metadata: true   # required for OTLP log attributes
  volume_enabled: true
  retention_period: 168h

compactor:
  working_directory: /var/lib/loki/compactor
  retention_enabled: true
  delete_request_store: filesystem
```

```bash
sudo mkdir -p /var/lib/loki/chunks /var/lib/loki/rules /var/lib/loki/compactor
sudo chown -R loki:loki /var/lib/loki /etc/loki/config.yml
sudo systemctl enable --now loki
curl -s http://localhost:3100/ready    # "ready" after ~15 s
```

---

## 12. Tempo 3.x (traces)

Tempo 3.0 removed the 2.x `ingester:` and `compactor:` blocks (block building
moved into the internal live-store, compaction to a job scheduler) and
**refuses to start** with the legacy flat `overrides:` format — the scoped
`defaults:` structure below is required. Monolithic mode needs no Kafka.

`/etc/tempo/config.yml`:

```yaml
stream_over_http_enabled: true

server:
  http_listen_port: 3200
  log_level: info

usage_report:
  reporting_enabled: false

distributor:
  receivers:
    otlp:
      protocols:
        # Alternate ports: Alloy owns 4317/4318 on this host.
        grpc:
          endpoint: "0.0.0.0:4319"
        http:
          endpoint: "0.0.0.0:4320"

# RED metrics + service graph derived from traces, pushed to Prometheus.
metrics_generator:
  registry:
    external_labels:
      source: tempo
  storage:
    path: /var/lib/tempo/generator/wal
    remote_write:
      - url: http://localhost:9090/api/v1/write
        send_exemplars: true

storage:
  trace:
    backend: local
    wal:
      path: /var/lib/tempo/wal
    local:
      path: /var/lib/tempo/blocks

# Scoped ("defaults") overrides format — REQUIRED in Tempo 3.0.
overrides:
  defaults:
    metrics_generator:
      processors:
        - service-graphs
        - span-metrics
```

```bash
sudo mkdir -p /var/lib/tempo/wal /var/lib/tempo/blocks /var/lib/tempo/generator/wal /var/tempo
sudo chown -R tempo:tempo /var/lib/tempo /var/tempo /etc/tempo/config.yml
sudo systemctl enable --now tempo
curl -s http://localhost:3200/ready
```

---

## 13. Alloy (collector) with secured OTLP ingest

Alloy is the single ingest point. The OTLP receiver is secured with **TLS**
(channel encryption) and a **bearer token** (caller authentication) so remote
applications can push safely.

### 13.1 TLS server certificate

Self-signed is fine for a test box; the hostname/IP the applications dial
**must** be in the SAN or client-side verification fails:

```bash
sudo mkdir -p /etc/alloy/tls
sudo openssl req -x509 -newkey rsa:4096 -nodes -days 365 \
  -keyout /etc/alloy/tls/server.key \
  -out    /etc/alloy/tls/server.crt \
  -subj "/CN=<ALLOY_HOST>" \
  -addext "subjectAltName=DNS:<ALLOY_HOST>,IP:<ALLOY_IP>"
sudo chown -R alloy:alloy /etc/alloy/tls
sudo chmod 600 /etc/alloy/tls/server.key
```

### 13.2 Bearer token (kept out of the config file)

This is the `otlp.token` from section 0.3 — the same value the collector
sends with `otlp-token`:

```bash
echo "OTLP_BEARER_TOKEN=$(cat /etc/xrootd/otlp.token)" | sudo tee -a /etc/sysconfig/alloy
sudo chmod 600 /etc/sysconfig/alloy
```

Optionally expose the Alloy UI beyond localhost in the same file:

```bash
sudo sed -i 's|^CUSTOM_ARGS=.*|CUSTOM_ARGS="--server.http.listen-addr=0.0.0.0:12345"|' /etc/sysconfig/alloy
```

### 13.3 `/etc/alloy/config.alloy`

```alloy
logging {
  level  = "info"
  format = "logfmt"
}

// ================= Auth handler for the OTLP receiver =================
otelcol.auth.bearer "otlp_in" {
  token = sys.env("OTLP_BEARER_TOKEN")
}

// ================= 1) OTLP in (TLS + bearer on both listeners) ========
otelcol.receiver.otlp "default" {
  grpc {
    endpoint = "0.0.0.0:4317"
    tls {
      cert_file = "/etc/alloy/tls/server.crt"
      key_file  = "/etc/alloy/tls/server.key"
    }
    auth = otelcol.auth.bearer.otlp_in.handler
  }
  http {
    endpoint = "0.0.0.0:4318"
    tls {
      cert_file = "/etc/alloy/tls/server.crt"
      key_file  = "/etc/alloy/tls/server.key"
    }
    auth = otelcol.auth.bearer.otlp_in.handler
  }

  output {
    metrics = [otelcol.processor.memory_limiter.default.input]
    logs    = [otelcol.processor.memory_limiter.default.input]
    traces  = [otelcol.processor.memory_limiter.default.input]
  }
}

// ================= 2) Reliability: memory cap, then batching ==========
otelcol.processor.memory_limiter "default" {
  check_interval         = "1s"
  limit_percentage       = 80
  spike_limit_percentage = 25

  output {
    metrics = [otelcol.processor.batch.default.input]
    logs    = [otelcol.processor.batch.default.input]
    traces  = [otelcol.processor.batch.default.input]
  }
}

otelcol.processor.batch "default" {
  output {
    metrics = [otelcol.exporter.otlphttp.metrics.input]
    logs    = [otelcol.exporter.otlphttp.logs.input]
    traces  = [otelcol.exporter.otlp.traces.input]
  }
}

// ================= 3) Exporters to the backends ========================
// Metrics -> Prometheus native OTLP receiver
otelcol.exporter.otlphttp "metrics" {
  client { endpoint = "http://localhost:9090/api/v1/otlp" }
}

// Logs -> Loki native OTLP endpoint.
// (For the OpenSearch alternative, see section 17: this exporter is
//  replaced by an OTLP gRPC exporter pointing at Data Prepper :21892.)
otelcol.exporter.otlphttp "logs" {
  client { endpoint = "http://localhost:3100/otlp" }
}

// Traces -> Tempo OTLP gRPC on the alternate port
otelcol.exporter.otlp "traces" {
  client {
    endpoint = "localhost:4319"
    tls { insecure = true }
  }
}

// ================= 4) Local host metrics ==============================
prometheus.exporter.unix "node" { }

prometheus.scrape "node_local" {
  targets    = prometheus.exporter.unix.node.targets
  forward_to = [prometheus.remote_write.default.receiver]
  job_name   = "node_alloy"
}

prometheus.remote_write "default" {
  endpoint {
    url = "http://localhost:9090/api/v1/write"
  }
}
```

```bash
sudo alloy fmt /etc/alloy/config.alloy > /dev/null && echo "config OK"
sudo systemctl enable --now alloy      # use `restart` after env-file changes
```

### 13.4 What the collector needs to push here

The OTLP client in this pipeline is `xrdmoncollect` — the section 3.2 keys
are all it takes:

```ini
otlp-url = https://<ALLOY_HOST>:4318
otlp-token = @/etc/xrootd/otlp.token
spans = true
```

Logs are POSTed to `<otlp-url>/v1/logs` and spans to `<otlp-url>/v1/traces`,
with `Authorization: Bearer <token>`. For the self-signed certificate above,
add `server.crt` to the collector host's CA trust (section 3.2 TLS note); in
the container tracks, bake it into the image (section 5.1).

Smoke test from the collector host:

```bash
curl -s -o /dev/null -w "%{http_code}\n" https://<ALLOY_HOST>:4318/v1/traces \
  --cacert server.crt                                   # 401 = TLS ok, auth enforced
curl -s -o /dev/null -w "%{http_code}\n" https://<ALLOY_HOST>:4318/v1/traces \
  --cacert server.crt -H "Authorization: Bearer $(cat /etc/xrootd/otlp.token)" \
  -X POST -H "Content-Type: application/json" -d '{}'   # 400 = auth ok (empty body)
```

> Any other OpenTelemetry SDK application can push to the same endpoint via
> the standard `OTEL_EXPORTER_OTLP_ENDPOINT` / `OTEL_EXPORTER_OTLP_HEADERS=`
> `Authorization=Bearer <token>` / `OTEL_EXPORTER_OTLP_CERTIFICATE` env vars.
>
> Stronger alternative: mTLS. Add `client_ca_file` to each `tls` block and
> drop the bearer auth; the client then authenticates with a certificate.

---

## 14. Grafana

### 14.1 Datasources

Provision datasources so cross-signal correlation works on first boot.

`/etc/grafana/provisioning/datasources/datasources.yaml`:

```yaml
apiVersion: 1
datasources:
  - name: Prometheus
    type: prometheus
    uid: prometheus
    access: proxy
    url: http://localhost:9090
    isDefault: true
    jsonData:
      httpMethod: POST
      exemplarTraceIdDestinations:
        - name: trace_id
          datasourceUid: tempo

  - name: Loki
    type: loki
    uid: loki
    access: proxy
    url: http://localhost:3100

  - name: Tempo
    type: tempo
    uid: tempo
    access: proxy
    url: http://localhost:3200
    jsonData:
      tracesToLogsV2:
        datasourceUid: loki
      serviceMap:
        datasourceUid: prometheus
      nodeGraph:
        enabled: true
```

### 14.2 XRootD dashboards

Three ready-made dashboards ship in the XRootD source tree next to this
guide (`src/XrdApps/XrdMonCollect/`):

| File | Datasource | Content |
|------|------------|---------|
| `grafana-dashboard.json` | Prometheus | collector health: rates, packet loss, sinks, per-VO/locality aggregates |
| `grafana-loki-dashboard.json` | Loki | transfer documents: throughput, clients, errors |
| `grafana-loki-popularity-dashboard.json` | Loki | data popularity by dataset/path |

Provision them as files:

```bash
sudo mkdir -p /var/lib/grafana/dashboards
sudo cp grafana-dashboard.json grafana-loki-dashboard.json \
        grafana-loki-popularity-dashboard.json /var/lib/grafana/dashboards/
sudo chown -R grafana:grafana /var/lib/grafana/dashboards
```

`/etc/grafana/provisioning/dashboards/xrootd.yaml`:

```yaml
apiVersion: 1
providers:
  - name: xrootd
    folder: XRootD
    type: file
    options:
      path: /var/lib/grafana/dashboards
```

```bash
sudo chown root:grafana /etc/grafana/provisioning/datasources/datasources.yaml \
                        /etc/grafana/provisioning/dashboards/xrootd.yaml
sudo systemctl enable --now grafana-server
```

Login at `http://<host>:3000` (`admin`/`admin`, forced change). Alerting:
Grafana unified alerting (Alerting menu) is used instead of a separate
Alertmanager.

---

## 15. Firewall

```bash
sudo firewall-cmd --permanent --add-port=3000/tcp     # Grafana
sudo firewall-cmd --permanent --add-port=4317/tcp     # OTLP gRPC (collectors / remote apps)
sudo firewall-cmd --permanent --add-port=4318/tcp     # OTLP HTTP (collectors / remote apps)
sudo firewall-cmd --permanent --add-port=12345/tcp    # Alloy UI (optional)
sudo firewall-cmd --reload
```

If the xrdmoncollect collector is co-located on this host, also open 9931
(shovel TCP, from the server subnet) and 9932 (self-metrics, local scrape
only — no rule needed) as in section 4.3.

---

## 16. Remote node_exporter hosts

`node_exporter` is **pull-based** — it exposes `:9100/metrics` and waits to be
scraped. Two patterns:

**A. Direct scrape (default, preferred).** Add the target to the `node` job in
`/etc/prometheus/prometheus.yml` (section 9) and reload Prometheus. On the
remote machine, open 9100 to the Prometheus host only:

```bash
sudo firewall-cmd --permanent \
  --add-rich-rule='rule family="ipv4" source address="<PROM_IP>" port port="9100" protocol="tcp" accept'
sudo firewall-cmd --reload
```

Verify at `http://<host>:9090/targets`.

**B. Push via a remote Alloy (only when inbound scraping is impossible, e.g.
NAT).** On the remote machine, a minimal Alloy scrapes locally and
remote-writes to central Prometheus:

```alloy
prometheus.scrape "node" {
  targets    = [{ "__address__" = "localhost:9100" }]
  forward_to = [prometheus.remote_write.central.receiver]
}

prometheus.remote_write "central" {
  endpoint {
    url = "http://<MONITORING_HOST>:9090/api/v1/write"
  }
}
```

(Alloy's built-in `prometheus.exporter.unix` can replace node_exporter
entirely on new machines.) Secure this link like the OTLP path if it crosses
an untrusted network.

---

## 17. Alternative log backend: OpenSearch (instead of Loki)

OpenSearch is a full-text search and analytics engine (Elasticsearch fork,
used at CERN and many large sites). Compared to Loki it indexes log *content*
(powerful ad-hoc search, higher resource cost), while Loki indexes only
labels (cheaper, LogQL-based). Both can also run side by side.

There are **two ways** to get XRootD documents into OpenSearch:

1. **Direct (preferred for xrdmoncollect):** the collector posts `_bulk`
   bodies straight to OpenSearch — `os-url` + `os-datastream` in section 3.2
   — with retries and the on-failure disk cache. No extra component needed.
2. **Via Alloy:** for OTLP logs routed through Alloy (or if you want a
   single ingest point for all telemetry). Alloy has **no native OpenSearch
   exporter**; the OTLP-native bridge is **Data Prepper** (17.3–17.4):

```
xrdmoncollect ──_bulk HTTPS+auth──────────────────► OpenSearch :9200 ◄── Grafana (plugin) /
Alloy ──OTLP gRPC :21892──► Data Prepper ──────────►                       OpenSearch Dashboards
```

> Sizing note: OpenSearch is a JVM application; budget at least 2 GB of heap
> (4 GB+ recommended) on top of the rest of the stack.

### 17.1 Install OpenSearch (RPM repo)

```bash
sudo curl -SL https://artifacts.opensearch.org/releases/bundle/opensearch/3.x/opensearch-3.x.repo \
  -o /etc/yum.repos.d/opensearch-3.x.repo

# 2.12+ requires a custom admin password at install time (sets up the demo
# security config: HTTPS on 9200 with self-signed certs + basic auth).
sudo env OPENSEARCH_INITIAL_ADMIN_PASSWORD='<StrongAdminPassword>' dnf -y install opensearch
```

Single-node settings — append to `/etc/opensearch/opensearch.yml`
(`network.host: 0.0.0.0` instead when the collector posts from another
host):

```yaml
discovery.type: single-node
network.host: 127.0.0.1
```

Cap the JVM heap for a test box — `/etc/opensearch/jvm.options.d/heap.options`:

```
-Xms2g
-Xmx2g
```

```bash
sudo sysctl -w vm.max_map_count=262144        # persisted by the RPM's sysctl.d file; verify
sudo systemctl enable --now opensearch
curl -ku admin:'<StrongAdminPassword>' https://localhost:9200   # cluster info JSON
```

### 17.2 XRootD index template + direct posting from the collector

Apply the composable index template shipped next to this guide **before the
first document arrives** — it declares `xrootd-transfers` a data stream and
maps the OpenTelemetry dotted attribute names:

```bash
curl -ku admin:'<StrongAdminPassword>' -X PUT \
  https://localhost:9200/_index_template/xrootd-transfers \
  -H 'Content-Type: application/json' \
  --data-binary @opensearch-template.json
```

Then enable the direct sink in the collector config (section 3.2):

```ini
os-url = https://<OPENSEARCH_HOST>:9200
os-index = xrootd-transfers
os-datastream = true
os-user = admin                    # better: a dedicated write-only user
os-pass = <StrongAdminPassword>
```

(For production, create a dedicated OpenSearch user with write access to
`xrootd-transfers*` only; `os-token = @<file>` sends a bearer token instead
if the security plugin is configured for JWT.) The demo TLS certificates are
self-signed — add them to the collector host's trust store, or
`os-insecure = true` for a lab.

Documents appear in the `xrootd-transfers` data stream; steps 17.3–17.4 are
then only needed if Alloy-routed logs should land in OpenSearch too.

### 17.3 Install Data Prepper (OTLP → OpenSearch bridge)

Data Prepper ships as a tarball with a bundled JDK (no RPM). Check the
current release on the OpenSearch downloads page and substitute the version:

```bash
DP_VERSION=<current 2.x version>     # e.g. 2.12.x — check opensearch.org/downloads
cd /opt
sudo curl -SLO https://artifacts.opensearch.org/data-prepper/${DP_VERSION}/opensearch-data-prepper-${DP_VERSION}-linux-x64.tar.gz
sudo tar xzf opensearch-data-prepper-${DP_VERSION}-linux-x64.tar.gz
sudo mv opensearch-data-prepper-${DP_VERSION}-linux-x64 data-prepper
sudo useradd --system --no-create-home --shell /sbin/nologin dataprepper || true
sudo chown -R dataprepper:dataprepper /opt/data-prepper
```

Pipeline — `/opt/data-prepper/pipelines/pipelines.yaml`:

```yaml
otel-logs-pipeline:
  source:
    otel_logs_source:
      port: 21892        # OTLP gRPC listener for logs
      ssl: false         # localhost-only link from Alloy; enable TLS if remote
  processor:
    - date:
        from_time_received: true
        destination: "@timestamp"
  sink:
    - opensearch:
        hosts: ["https://localhost:9200"]
        username: admin
        password: "<StrongAdminPassword>"
        insecure: true               # demo self-signed certs; use cert paths in prod
        index: otel-logs-%{yyyy.MM.dd}
```

Systemd unit — `/etc/systemd/system/data-prepper.service`:

```ini
[Unit]
Description=OpenSearch Data Prepper
Wants=network-online.target opensearch.service
After=network-online.target opensearch.service

[Service]
User=dataprepper
Group=dataprepper
WorkingDirectory=/opt/data-prepper
ExecStart=/opt/data-prepper/bin/data-prepper
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now data-prepper
journalctl -u data-prepper -n 30 --no-pager
```

### 17.4 Point Alloy's log pipeline at Data Prepper

In `/etc/alloy/config.alloy`, replace the Loki log exporter with an OTLP gRPC
exporter, and update the batch processor's `logs` route:

```alloy
// was: otelcol.exporter.otlphttp "logs" { ... :3100/otlp ... }
otelcol.exporter.otlp "logs_opensearch" {
  client {
    endpoint = "localhost:21892"
    tls { insecure = true }     // local-only hop
  }
}
```

```alloy
otelcol.processor.batch "default" {
  output {
    metrics = [otelcol.exporter.otlphttp.metrics.input]
    logs    = [otelcol.exporter.otlp.logs_opensearch.input]
    traces  = [otelcol.exporter.otlp.traces.input]
  }
}
```

(To run Loki **and** OpenSearch in parallel, list both exporters in the
`logs` array instead of replacing one.)

```bash
sudo alloy fmt /etc/alloy/config.alloy > /dev/null && echo OK
sudo systemctl restart alloy
```

### 17.5 Query OpenSearch from Grafana

```bash
sudo grafana-cli plugins install grafana-opensearch-datasource
sudo systemctl restart grafana-server
```

Add the datasource (Connections → Data sources → OpenSearch):

- URL `https://localhost:9200`, Basic auth `admin` / password,
  **Skip TLS verify** enabled (demo certs).
- For collector documents: index name `xrootd-transfers*`, time field
  `@timestamp`.
- For Alloy-routed logs: index name `otel-logs-*`, time field `@timestamp`.

Verify end to end with a real transfer (section 20), or re-run the
`telemetrygen logs` command from section 20.1 and query Lucene `*` over the
last 15 minutes in Grafana → Explore → OpenSearch.

### 17.6 Optional: OpenSearch Dashboards

For the OpenSearch-native UI (the CERN-style experience) instead of, or next
to, Grafana:

```bash
sudo curl -SL https://artifacts.opensearch.org/releases/bundle/opensearch-dashboards/3.x/opensearch-dashboards-3.x.repo \
  -o /etc/yum.repos.d/opensearch-dashboards-3.x.repo
sudo dnf -y install opensearch-dashboards
sudo systemctl enable --now opensearch-dashboards
# http://localhost:5601 — login admin / <StrongAdminPassword>
```

Import the ready-made XRootD dashboards shipped next to this guide
(`opensearch-dashboards.ndjson` — transfer monitoring;
`opensearch-popularity.ndjson` — data popularity):

```bash
curl -ku admin:'<StrongAdminPassword>' -X POST \
  "http://localhost:5601/api/saved_objects/_import?overwrite=true" \
  -H "osd-xsrf: true" --form file=@opensearch-dashboards.ndjson
curl -ku admin:'<StrongAdminPassword>' -X POST \
  "http://localhost:5601/api/saved_objects/_import?overwrite=true" \
  -H "osd-xsrf: true" --form file=@opensearch-popularity.ndjson
```

---

# Part III — Storage, operations, verification

## 18. Long-term storage for container deployments

Native packages write to well-known host paths and survive service restarts
by construction. In containers, **anything not on a volume is lost when the
container is replaced** — which for a monitoring stack means metric history,
log/document history, dashboards, and the resilience spools. This section
covers what must persist and how, for the podman (5), compose (6) and
Kubernetes (7) tracks.

### 18.1 What must persist

| Service | Path (in container) | Content | If lost | Sizing start point |
|---------|--------------------|---------|---------|--------------------|
| Prometheus | `/prometheus` | TSDB | metric history gone | grows with series count × retention; 50 Gi for 90 d on a mid-size site, then measure |
| Loki | `/var/lib/loki` | chunks + index | document history gone | dominated by transfer volume (~1–2 KiB/document before compression) |
| Tempo | `/var/lib/tempo` | WAL + blocks | trace history gone | spans are opt-in; start 20 Gi |
| Grafana | `/var/lib/grafana` | `grafana.db` (users, manual dashboards), plugins | dashboards/users gone (provisioned ones come back) | 1 Gi |
| OpenSearch | `/usr/share/opensearch/data` | indices / data streams | document history gone | the largest consumer — plan with the ISM policy below |
| collector | `/var/lib/xrootd` | on-failure spool (`cache-dir`) + correlation state (`state-file`) | buffered unsent documents dropped; blind window after restart while dictionaries repopulate | ≥ 10 Gi (spool must cover your longest tolerated backend outage) |
| shoveler | `/var/lib/xrootd` | relay spool (`<cache-dir>/shovel`) | datagrams buffered during collector outage dropped | `spool-max` (default 1 G) + margin |

Configs and token files are *inputs*, not state — keep them on the host (or
in ConfigMaps/Secrets) under configuration management, mounted read-only.

### 18.2 podman / compose: named volumes vs bind mounts

Prefer **named volumes** for data (as in the section 6 compose file):
podman/docker manage their lifecycle and SELinux labels, and they survive
`docker compose down` and image upgrades. Reserve **bind mounts** for
read-only configs (`:ro,Z`) — and for data only when it must land on a
specific filesystem (a RAID array, a big-disk mount); then label with `:Z`
and own the directory to the container user.

```bash
podman volume ls
podman volume inspect prometheus-data     # find the host path (Mountpoint)
podman volume export collector-state | zstd > collector-state.tar.zst   # ad-hoc backup
```

> **`docker compose down -v` / `podman volume rm` delete the volumes.**
> Never use `-v` on a production stack; script your teardown without it.

To place all named volumes on a dedicated filesystem, configure the volume
root once (`/etc/containers/storage.conf` `volumepath` for podman, or create
each volume with `--opt device=... --opt o=bind`).

### 18.3 Kubernetes: PersistentVolumeClaims

Every stateful component gets its own PVC (the collector's is in 7.2; the
Helm charts of section 7.4 create theirs when persistence is enabled —
always set it explicitly, several charts default to `emptyDir`!):

```yaml
# example values.yaml fragments
# kube-prometheus-stack:
prometheus:
  prometheusSpec:
    retention: 90d
    storageSpec:
      volumeClaimTemplate:
        spec:
          storageClassName: <site-class>
          resources:
            requests:
              storage: 50Gi
# grafana:
persistence:
  enabled: true
  size: 1Gi
```

Recommendations:

- Pick a `storageClassName` backed by local SSD or a resilient network
  volume; monitoring I/O is write-heavy and latency-sensitive (Prometheus
  WAL, OpenSearch translog).
- Set `reclaimPolicy: Retain` on the StorageClass (or patch the PVs) for
  Prometheus and OpenSearch, so deleting a release or PVC by accident does
  not destroy the history.
- Use a StorageClass with `allowVolumeExpansion: true`; monitoring storage
  is the thing you *will* resize.
- The collector Deployment uses `strategy: Recreate` (7.2) because its PVC
  is `ReadWriteOnce` — one writer, no rolling update overlap.

### 18.4 Retention — the knob per store

Retention belongs to the *store*, not the volume. Set it deliberately in
each backend, sized to the volume you provisioned:

| Store | Knob | Where |
|-------|------|-------|
| Prometheus | `--storage.tsdb.retention.time=90d` (and/or `.size=45GB`) | unit flags (9) / compose command (6) / chart values (18.3) |
| Loki | `limits_config.retention_period` + `compactor.retention_enabled` | section 11 config (168 h shown — raise for production) |
| Tempo | block retention (default 14 d) | Tempo 3.x storage settings — the 2.x `compactor:` block is gone; consult the 3.x docs for the current key |
| OpenSearch | ISM policy (below) | applied once via the API |
| collector | `spool-max` (shovel spool), `max-memory`/`server-ttl` (state) | section 3 configs |
| Grafana | none (config database) | back it up instead (18.5) |

OpenSearch ISM policy — roll the `xrootd-transfers` data stream daily and
delete backing indices after 180 days (adjust ages; verify against your
OpenSearch version's ISM schema):

```bash
curl -ku admin:'<StrongAdminPassword>' -X PUT \
  https://localhost:9200/_plugins/_ism/policies/xrootd-retention \
  -H 'Content-Type: application/json' -d '{
  "policy": {
    "description": "xrootd-transfers: daily rollover, delete after 180d",
    "default_state": "hot",
    "states": [
      {
        "name": "hot",
        "actions": [{ "rollover": { "min_index_age": "1d" } }],
        "transitions": [
          { "state_name": "delete", "conditions": { "min_index_age": "180d" } }
        ]
      },
      {
        "name": "delete",
        "actions": [{ "delete": {} }],
        "transitions": []
      }
    ],
    "ism_template": [
      { "index_patterns": ["xrootd-transfers*", "otel-logs-*"], "priority": 100 }
    ]
  }
}'
```

### 18.5 Backups

- **Prometheus**: enable the admin API (`--web.enable-admin-api`), then
  `curl -X POST localhost:9090/api/v1/admin/tsdb/snapshot` and archive the
  snapshot directory. Metric history is usually acceptable to lose — decide
  per site.
- **Grafana**: back up `/var/lib/grafana/grafana.db` (or the volume), or —
  better — keep every dashboard provisioned from files in git and treat the
  DB as disposable.
- **OpenSearch**: register a snapshot repository (`fs` type on a mounted
  backup volume, or S3) and schedule snapshots with Snapshot Management.
  This is the one store whose loss usually hurts (accounting history).
- **Loki / Tempo**: filesystem copies of the volumes while stopped; for
  serious durability move their storage backend to object storage (S3/MinIO)
  instead of backing up local disks.
- **Spools and correlation state** (collector/shoveler volumes): transient
  by design — replayed or rebuilt within minutes. No backup needed.

## 19. Operations

### 19.1 Token rotation

Rotate one hop at a time, **receiver first** — the sender's buffering
covers the mismatch window:

1. **shovel token**: write the new value into `/etc/xrootd/shovel.token` on
   the collector host, restart the collector (shovelers spool during the
   restart, then get rejected + retry every 30 s), then roll the file out to
   the shoveler nodes and restart the shovelers — spools replay, nothing is
   lost.
2. **otlp token**: update `OTLP_BEARER_TOKEN` in `/etc/sysconfig/alloy` and
   restart Alloy; the collector's POSTs fail (401) and spool to `cache-dir`;
   update `/etc/xrootd/otlp.token` and restart the collector; the cache
   replays.
3. **metrics token**: update `metrics.authtoken` in the xrootd config and
   the Prometheus scrape job together; a few failed scrapes in between are
   harmless.

Token files are read at startup — a restart (not a reload) applies them.

### 19.2 Restarts and upgrades

- **Native collector/shoveler restarts are zero-loss** while the socket
  unit is enabled: systemd keeps the UDP/TCP sockets open and the kernel
  buffers datagrams (16 M default) across the restart. Correlation state is
  saved on shutdown and restored on start (`state-file`, discarded beyond
  `state-ttl`), so a clean restart does not open a blind window.
- **Containerized collectors** have no socket activation — the shovelers'
  spool is the restart cover. Keep collector restarts short and don't
  restart all shovelers at the same time as the collector.
- Upgrade order for the pipeline: backend first (Alloy buffers), then the
  collector (shovelers buffer), then shovelers (kernel buffer covers
  seconds), then XRootD servers at your own pace.

### 19.3 What to alert on

All from the `moncollect` Prometheus job (section 9):

| Alert | Expression sketch | Meaning |
|-------|-------------------|---------|
| collector/shoveler down | `up{job="moncollect"} == 0` | pipeline blind |
| packet loss | `rate(xrootd_collector_packets_lost_total[10m]) > 0` | UDP loss — check MTU/`fbsz` (section 2.1) and `rcvbuf` |
| malformed input | `rate(xrootd_collector_malformed_total[10m]) > 0` | truncation or stray traffic on the port |
| sink failing | `rate(xrootd_collector_post_failures_total[10m]) > 0` or the `otlp_*` failure counters | backend unreachable — spool is absorbing |
| spool backlog | `xrootd_collector_cache_files` growing for >1 h | backend outage outlasting the buffer |
| shovel spool dropping | `rate(xrootd_shoveler_spool_dropped_total[10m]) > 0` | outage exceeded `spool-max` — data loss |
| state pressure | `xrootd_collector_evicted_total` climbing | raise `max-memory` or lower `server-ttl` |

Loss on the `f` stream alone (label `stream="f"`), with other streams clean,
is the fragmentation signature — some server is still emitting 64 KiB fstat
datagrams (section 2.1).

## 20. End-to-end verification

Three layers, from the backend inward. Container equivalents in 20.4.

### 20.1 Backend synthetic (no XRootD needed)

```bash
go install github.com/open-telemetry/opentelemetry-collector-contrib/cmd/telemetrygen@latest

TOKEN=$(cat /etc/xrootd/otlp.token)
~/go/bin/telemetrygen traces  --otlp-endpoint <ALLOY_HOST>:4317 \
  --ca-cert server.crt --otlp-header "Authorization=\"Bearer ${TOKEN}\"" --traces 20
~/go/bin/telemetrygen metrics --otlp-endpoint <ALLOY_HOST>:4317 \
  --ca-cert server.crt --otlp-header "Authorization=\"Bearer ${TOKEN}\"" --metrics 20
~/go/bin/telemetrygen logs    --otlp-endpoint <ALLOY_HOST>:4317 \
  --ca-cert server.crt --otlp-header "Authorization=\"Bearer ${TOKEN}\"" --logs 20
```

In Grafana → Explore:

- Prometheus: `up`, `node_load1`; `traces_spanmetrics_calls_total` after traces flow.
- Loki: `{service_name="telemetrygen"}`.
- Tempo: Search → recent traces; Service Graph populates from span metrics.

### 20.2 Real XRootD traffic

```bash
xrdcp /etc/hostname xroot://<XROOTD_HOST>:1094//<export>/mon-smoke-test
xrdcp xroot://<XROOTD_HOST>:1094//<export>/mon-smoke-test /tmp/back
```

Follow the transfer through the pipeline:

1. **Shoveler** (`curl -s <node>:9932/metrics`): the shoveler frame counters
   increase; the spool gauges stay at zero.
2. **Collector** (`curl -s <collector>:9932/metrics`):
   `xrootd_collector_packets_total` increases;
   `packets_lost_total`/`malformed_total` stay flat; the transfer aggregates
   (per-VO/locality counters) tick after the file close arrives (up to one
   `fstat` interval — 60 s with the section 2.1 config).
3. **Documents**: Grafana → Explore → Loki,
   `{service_name="xrootd"} |= "mon-smoke-test"` — one
   `xrootd.transfer` document with the file path, client, byte counts and
   `wlcg.site` (OpenSearch path: Lucene
   `attributes.file.path:*mon-smoke-test*` on `xrootd-transfers*`).
4. **Traces** (with `spans = true`): Tempo → Search — a file-operation span
   whose duration matches open→close.
5. **Server metrics**: Prometheus `xrootd_*` series from the `xrootd` job
   (section 2.3).

### 20.3 Authentication checks

Verify every hop *rejects* as designed:

```bash
# Alloy OTLP without token -> 401; with token -> 400 on an empty body
curl -s -o /dev/null -w "%{http_code}\n" --cacert server.crt https://<ALLOY_HOST>:4318/v1/logs
curl -s -o /dev/null -w "%{http_code}\n" --cacert server.crt https://<ALLOY_HOST>:4318/v1/logs \
  -H "Authorization: Bearer $(cat /etc/xrootd/otlp.token)" \
  -X POST -H "Content-Type: application/json" -d '{}'

# xrootd /metrics without token -> 401 + WWW-Authenticate; with token -> 200
curl -s -o /dev/null -w "%{http_code}\n" http://<XROOTD_HOST>:8443/metrics
curl -s -o /dev/null -w "%{http_code}\n" http://<XROOTD_HOST>:8443/metrics \
  -H "Authorization: Bearer $(cat /etc/xrootd/metrics.token)"
```

Shovel hop: put a wrong value in a shoveler's `shovel-token` file and
restart it — the collector rejects the hello, and the shoveler logs the
rejection and backs off for 30 s between attempts (a network failure, by
contrast, retries every 5 s). Frames spool meanwhile; restore the token and
watch `xrootd_shoveler_spool_replayed_total` drain the backlog.

### 20.4 Container equivalents

| Check | podman / compose | Kubernetes |
|-------|------------------|------------|
| service logs | `podman logs -f moncollect-collector` / `docker compose logs -f collector` | `kubectl -n xrootd-monitoring logs -f deploy/moncollect-collector` |
| collector metrics | `curl -s localhost:9932/metrics` (published port) | `kubectl -n xrootd-monitoring port-forward svc/moncollect 9932:9932` then curl |
| shoveler reaches collector | `podman exec moncollect-shoveler curl -s localhost:9932/metrics \| grep shovel` | `kubectl exec` into the sidecar/DaemonSet pod, same curl |
| spool inspection | `podman exec moncollect-collector ls /var/lib/xrootd/moncollect` | `kubectl exec deploy/moncollect-collector -- ls /var/lib/xrootd/moncollect` |
| stack state | `docker compose ps` (healthchecks) | `kubectl -n xrootd-monitoring get pods,pvc` |

---

## Appendix: service cheat-sheet

```bash
# Logs
journalctl -u <xrootd@*|xrdmoncollect|prometheus|loki|tempo|alloy|grafana-server|opensearch|data-prepper> -f

# Config validation / reload
sudo alloy fmt /etc/alloy/config.alloy         # Alloy syntax check
curl -X POST http://localhost:9090/-/reload    # Prometheus config reload

# Readiness
curl -s localhost:9932/metrics | head          # xrdmoncollect (collector or shoveler)
curl -s localhost:9090/-/ready                 # Prometheus
curl -s localhost:3100/ready                   # Loki
curl -s localhost:3200/ready                   # Tempo
curl -ku admin:<pw> https://localhost:9200     # OpenSearch

# xrdmoncollect state
ss -ulpn 'sport = 9930'                        # UDP socket (systemd-owned when activated)
ls /var/lib/xrootd/moncollect                  # on-failure spool backlog
ls /var/lib/xrootd/moncollect/shovel           # shoveler relay spool

# Containers
podman ps; podman volume ls                    # Track B
docker compose ps; docker compose logs -f      # Track C
kubectl -n xrootd-monitoring get pods,svc,pvc  # Track D
```

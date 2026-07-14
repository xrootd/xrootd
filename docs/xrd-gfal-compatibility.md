# Native XRootD CLI migration compatibility

This document records the behavior of the first migration frontend prototype
tracked by [issue #2863](https://github.com/xrootd/xrootd/issues/2863).
`xrd` is a working name only. The final executable name, syntax, packaging,
and installation policy remain open for review.

The frontend is intentionally thin: it translates arguments and environment
settings, then replaces itself with `xrdfs` or `xrdcp`. Storage operations,
protocol handling, transfer progress, normal output, signals, and exit status
remain owned by the native commands.

The prototype target is built for review and testing but is deliberately not
installed or added to distribution packages while its name and packaging
policy remain unresolved.

The compatibility inventory is based on the captured `gfal2-util` help and
behavioral tests maintained in the
[`lobis/gfal`](https://github.com/lobis/gfal) reference repository.

## Implemented first slice

| Migration command | Delegated invocation | Supported compatibility behavior |
| --- | --- | --- |
| `xrd stat URL` | `xrdfs stat URL` | Command name and complete URL |
| `xrd cat URL...` | `xrdfs cat URL...` | Multiple same-endpoint files; `-b/--bytes` is a safe no-op |
| `xrd ls URL` | `xrdfs ls URL` | `-a`, `-l`, `-H`, and non-coloring modes |
| `xrd xattr URL [NAME]` | `xrdfs xattr URL list/get` | Read-only list and get; assignment is rejected |
| `xrd copy SRC DST` | `xrdcp SRC DST` | `-f`, `-p`, `-r`, `-K`, `-n`, `-T`, and `--from-file` |
| `xrd cp SRC DST` | `xrdcp SRC DST` | Alias for `copy` |

Complete-URL dispatch depends on the `xrdfs` command-first URL grammar from
issue #1664. The existing server-first and interactive `xrdfs` forms remain
unchanged.

## Common option translation

| GFAL spelling | Native setting |
| --- | --- |
| `-v` (repeatable) | `XRD_LOGLEVEL=Warning`, `Info`, or `Debug` |
| `-t/--timeout SECONDS` | `XRD_REQUESTTIMEOUT`; zero is accepted without forcing XrdCl's immediate-expiry value |
| `-E/--cert FILE` | Explicit GSI/native/HTTP certificate, key, and proxy selection; inherited proxy and serialized XrdSec credentials are cleared |
| `--key FILE` | Override the key selected by `--cert`; otherwise the certificate file is also used as the key |
| `-4` / `-6` | `XRD_NETWORKSTACK=IPv4` / `IPv6` |
| `--log-file FILE` | `XRD_LOGFILE` |
| `-D/--definition`, `-C/--client-info` | accepted with an explicit warning |

Options are translated only when their native behavior is sufficiently close.
Unsupported options fail before delegation with a specific diagnostic. This
avoids silently claiming SRM, GridFTP, formatting, or third-party-copy behavior
that XRootD does not provide through the selected command.

## Deliberate current boundaries

- Output is native `xrdfs`/`xrdcp` output. In particular, `stat` and long
  `ls` are not formatted to look like `gfal-stat` or `gfal-ls`.
- Exit status is the delegated native status. Exact GFAL POSIX-errno mapping is
  a later script-compatibility layer.
- `sum` is deferred until a native command accepts a complete URL and checksum
  algorithm without making the frontend split URLs or parse output.
- `xattr` exposes read-only get/list only. An attribute containing `=` is
  rejected so mutation cannot be introduced accidentally.
- Copy accepts a single source and destination, or `--from-file` plus one
  destination. GFAL chained destinations are rejected because passing them
  unchanged to `xrdcp` would mean multiple sources and change semantics.
- `--` protects dash-prefixed local copy operands; they are normalized to an
  equivalent `./-name` spelling before delegation because `xrdcp` does not
  preserve the delimiter itself.
- `ls -d`, time styles, xattr columns, and forced color need native `xrdfs`
  support before the frontend can offer them.
- HTTP(S) behavior depends on the installed `XrdClHttp` plugin and its native
  operation coverage. The dispatcher does not add protocol-specific code.
- Against the EOS Public reference fixture, native HTTPS `stat`, `cat`, and
  remote-to-local copy succeed, while HTTPS `ls` currently returns
  `NOT_IMPLEMENTED`; `gfal-ls` succeeds on the same directory. Directory
  listing must therefore be completed in XrdClHttp/`xrdfs`, not reconstructed
  in this frontend.
- Multi-file `cat` currently requires all URLs to use one endpoint because one
  delegated `xrdfs` invocation owns the operation. Unlike `gfal-cat`, it does
  not yet span unrelated endpoints.

## Validation policy

The in-tree unit and BATS tests validate argument/environment translation,
delegation, stream preservation, and status preservation without contacting
storage.

Server-backed tests use an isolated local XRootD fixture. Live acceptance checks
must use read-only public data or download to a local temporary file. The
reference fixture is:

```text
root://eospublic.cern.ch//eos/opendata/phenix/emcal-finding-pi0s-and-photons/single_cluster_r5.C
```

Its expected size is 2184 bytes, MD5 is
`93f402e24c6f870470e1c5fcc5400e25`, and ADLER32 is `335e754f`.
No validation should create, rename, chmod, stage, evict, or remove data on
production storage.

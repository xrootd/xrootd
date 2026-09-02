# Using XRootD tools for read-only gfal2-util workflows

This document covers the first compatibility slice for operators moving common
read-only workflows from gfal2-util to XRootD tools.

The metadata implementation stays inside `xrdfs`. It does not add an `xrd`
application, a wrapper, or a second execution layer. Complete URLs and
compatibility options are normalized and passed to the existing `xrdfs`
command handlers and XrdCl operations. Read-only copies use the existing
`xrdcp` application; this work does not add another copy engine.

This is a migration aid, not a promise of byte-for-byte gfal2-util
compatibility.

## Complete URL operands

Traditional `xrdfs` syntax separates the endpoint from the command and path:

```console
xrdfs root://storage.example.org stat //store/data/file.dat
```

The same operation can now be written in the command-first form used by
gfal2-util:

```console
xrdfs stat root://storage.example.org//store/data/file.dat
```

The traditional form remains supported.

When one invocation contains multiple complete URLs, they must all use the same
endpoint. `xrdfs` extracts that endpoint once and executes the existing command
against the normalized paths. Mixed endpoints are rejected before any remote
operation is attempted.

## Command mapping

Assume these example operands:

```sh
DIR='root://storage.example.org//store/data/'
FILE_A='root://storage.example.org//store/data/a.dat'
FILE_B='root://storage.example.org//store/data/b.dat'
```

| gfal2-util | `xrdfs` | Compatibility provided |
| --- | --- | --- |
| `gfal-stat "$FILE_A"` | `xrdfs stat "$FILE_A"` | Stats a complete URL using the existing `xrdfs stat` implementation. |
| `gfal-ls "$DIR"` | `xrdfs ls "$DIR"` | Lists the contents of the directory. |
| `gfal-ls -lH "$DIR"` | `xrdfs ls -lH "$DIR"` | Accepts the gfal-style `-H` human-readable-size option, including grouped `-lH`. The existing `xrdfs ls -h` spelling remains supported. |
| `gfal-ls -d "$DIR"` | `xrdfs ls -d "$DIR"` | Prints the directory operand itself instead of listing its contents. |
| `gfal-ls -a "$DIR"` | `xrdfs ls -a "$DIR"` | Accepts `-a` / `--all` as a compatibility no-op because native `xrdfs ls` already includes dotfiles. |
| `gfal-ls --color=never "$DIR"` | `xrdfs ls --color=never "$DIR"` | Accepts the explicitly uncolored form as a compatibility no-op. |
| `gfal-ls -l --xattr user.status "$FILE_A"` | `xrdfs ls -l --xattr user.status "$FILE_A"` | Appends the requested virtual or native attribute value to long output. The option is repeatable and preserves order. As in GFAL, it has no visible effect without `-l`. |
| `gfal-cat -b "$FILE_A"` | `xrdfs cat -b "$FILE_A"` | Accepts `-b` as a compatibility no-op because `xrdfs cat` already writes file data to standard output without text conversion. |
| `gfal-cat -b "$FILE_A" "$FILE_B"` | `xrdfs cat -b "$FILE_A" "$FILE_B"` | Concatenates multiple files from the same endpoint. |
| `gfal-sum "$FILE_A" ADLER32` | `xrdfs sum "$FILE_A" ADLER32` | Selects the requested checksum algorithm and validates that the server returned that algorithm. |
| `gfal-xattr "$FILE_A"` | `xrdfs xattr "$FILE_A"` | Lists the GFAL virtual XRootD attributes available through existing checksum and space queries. |
| `gfal-xattr "$FILE_A" xroot.cksum` | `xrdfs xattr "$FILE_A" xroot.cksum` | Resolves a GFAL virtual attribute through the existing XrdCl checksum query. |
| `gfal-xattr "$FILE_A" user.checksum.adler32` | `xrdfs xattr "$FILE_A" user.checksum.adler32` | Selects the requested checksum algorithm and prints only its digest. |
| `gfal-xattr "$FILE_A" user.status` | `xrdfs xattr "$FILE_A" user.status` | Derives GFAL's disk/tape status value from the existing XrdCl stat result. |

Known GFAL virtual attribute names use a thin adapter implemented with
operations that already exist in XrdCl:

| Virtual attribute | Existing XrdCl operation |
| --- | --- |
| `xroot.cksum` | Default checksum query; returns the algorithm and digest. |
| `user.checksum.<algorithm>` | Checksum query selecting `<algorithm>`; returns the digest. |
| `xroot.space` | Space query. |
| `xroot.xattr` | XRootD xattr query. |
| `spacetoken` | Existing space-info helper, formatted as the GFAL JSON object. |
| `user.status` | Stat query; `Offline` and `BackUpExists` map to `ONLINE`, `NEARLINE`, `ONLINE_AND_NEARLINE`, or `UNKNOWN`. |

The bare shorthand list is available for XRootD protocols and queries the four
fixed attributes `xroot.cksum`, `xroot.space`, `xroot.xattr`, and `spacetoken`.
`user.checksum.<algorithm>` and `user.status` are requested by name rather than
included in that list. The checksum shorthand can also work through another
protocol plugin, such as XrdClHttp, when that plugin supports the checksum
query. The other listed virtual attributes are specific to XRootD protocols.

An unrecognized single attribute name falls back to the native XRootD get
operation and prints its raw value. This preserves the existing shorthand for
native file attributes. Names that collide with the explicit operation words
can be written after `--`, for example `xrdfs xattr "$FILE_A" -- list`.

Use the explicit forms to access native XRootD file attributes:

```console
xrdfs xattr "$FILE_A" list
xrdfs xattr "$FILE_A" get user.example
```

These dispatch the native file-attribute list and get operations; they do not
enumerate or resolve the GFAL virtual values above. Native set and delete remain
available only through their explicit forms and are outside this read-only
compatibility scope.

HTTP(S) Tape REST virtual attributes exposed by XrdClHttp are a separate
feature. They are not enumerated by this shorthand and are not claimed as part
of the GFAL virtual-attribute compatibility described here.

## Remote-to-local copies with `xrdcp`

`gfal-copy` and `gfal-cp` downloads map to the existing `xrdcp` application.
Assume `LOCAL=/tmp/a.dat`, `LOCAL_DIR=/tmp/downloads`, and `INPUTS` names a file
containing one source URL per line:

| gfal2-util download | Existing `xrdcp` form | Notes |
| --- | --- | --- |
| `gfal-copy "$FILE_A" "file://$LOCAL"` | `xrdcp "$FILE_A" "file://$LOCAL"` | Copies one remote file to a local path. A bare local path is also accepted. |
| `gfal-copy -f "$FILE_A" "file://$LOCAL"` | `xrdcp -f "$FILE_A" "file://$LOCAL"` | Replaces an existing local destination. |
| `gfal-copy -p "$FILE_A" "file://$LOCAL"` | `xrdcp -p "$FILE_A" "file://$LOCAL"` | Creates missing destination path components. |
| `gfal-copy -n 4 "$FILE_A" "file://$LOCAL"` | `xrdcp -S 4 "$FILE_A" "file://$LOCAL"` | Uses four data streams. |
| `gfal-copy -K ADLER32 "$FILE_A" "file://$LOCAL"` | `xrdcp --rm-bad-cksum -C adler32 "$FILE_A" "file://$LOCAL"` | Verifies the destination and removes it if verification fails. |
| `gfal-copy --from-file "$INPUTS" "file://$LOCAL_DIR/"` | `xrdcp --infiles "$INPUTS" "file://$LOCAL_DIR/"` | Reads source URLs from a file and uses one existing destination directory. |
| No reliable `gfal-copy` stdout form | `xrdcp "$FILE_A" -` | `xrdcp` supports stdout as a native superset. |

`--rm-bad-cksum` is important in the checksum mapping: `-C` enables checksum
verification, while `--rm-bad-cksum` gives the expected cleanup behavior when
verification fails.

The following copy forms are not mechanically equivalent:

- Recursive directory layout differs. `gfal-copy -r` copies the source
  contents to the requested destination and preserves empty directories,
  whereas `xrdcp -r` can require an existing destination directory, nest the
  source basename below it, and omit empty directories. Symlink handling also
  differs. Review the resulting layout before migrating a recursive script.
- Multiple GFAL destination operands form a chain (`src` to `dst1`, then
  `dst1` to `dst2`). Multiple `xrdcp` source operands and `--infiles` instead
  copy several sources into one destination directory.
- When an input list contains a missing source, legacy `gfal-copy --from-file`
  stops at that source, while `xrdcp --infiles` continues its independent jobs
  and still returns failure. A later valid source may therefore be downloaded
  only by `xrdcp`.
- `gfal-copy --dry-run` has no `xrdcp` equivalent.

For a local destination, GFAL's `streamed`, `pull`, and `push` copy modes all
produced the same bytes in the reference tests; ordinary `xrdcp` is already a
client-streamed download. GFAL's global transfer timeout (`-T`) has no exact
`xrdcp` wall-time option, and protocol-specific source/destination space-token
or TCP-buffer options do not have a general remote-to-local mapping.

Only downloads to a local path or stdout are in scope here. Uploads and any
other copy with a remote destination, including third-party copies, are not
part of this read-only compatibility work.

## Testing approach

The XRootD tests exercise the compatibility behavior against a local XRootD
server and controlled fixtures. They cover:

- complete-URL parsing and preservation of URL parameters;
- legacy server-first syntax;
- `stat`;
- normal, long human-readable, and directory-entry `ls`;
- binary-safe and multiple-file `cat`;
- algorithm-selecting checksum queries through `sum`;
- GFAL virtual xattr shorthand and explicit native xattr list/get;
- remote-to-local `xrdcp` downloads, including overwrite, checksum validation,
  input lists, and stdout;
- rejection of local URLs and mixed remote endpoints.

The expected behavior is derived from the corresponding gfal2-util commands,
but gfal2 is not a build-time or runtime dependency of `xrdfs`, and it is not
required to run the XRootD test suite. The tests compare operation semantics
and relevant data, not exact diagnostic or presentation formatting.

The normal test run registers the reference suite but skips its comparisons.
On a host with functional gfal2-util commands, enable the controlled local
comparison with:

```console
XRDFS_GFAL2_REFERENCE=1 \
  ctest --test-dir build --output-on-failure \
  -R '^XrdCl::xrdfs-gfal2-reference$'
```

The enabled suite starts its own localhost XRootD server, uses generated test
data, and writes copy destinations only below the test temporary directory.
Optional read-only protocol comparisons can also be enabled by setting all
four of these variables to equivalent accessible fixtures:

```sh
XRDFS_GFAL2_LIVE_ROOT_FILE='root://host.example//path/file'
XRDFS_GFAL2_LIVE_HTTPS_FILE='https://host.example/path/file'
XRDFS_GFAL2_LIVE_ROOT_DIR='root://host.example//path/directory/'
XRDFS_GFAL2_LIVE_HTTPS_DIR='https://host.example/path/directory/'
```

The live cases compare ROOT, HTTPS, and the DAVS alias. They perform only
metadata queries and reads; downloads remain local.

## Compatibility boundaries

The output remains native `xrdfs` output. In particular, `stat`, long `ls`, and
xattr formatting may differ from gfal2-util in field names, ordering, path
presentation, size rounding, timestamps, and diagnostics. Scripts that parse
gfal2-util output should not assume identical text.

Exit statuses also remain XRootD statuses. Portable migration code should
distinguish success from failure rather than depending on a particular nonzero
value matching gfal2-util.

Unsupported `gfal-ls` presentation options, including `--time-style`,
`--full-time`, and colored `auto` or `always` output, are rejected
instead of being silently interpreted as path operands. Native `xrdfs ls`
formatting remains available through its existing options.

Accepting a complete URL does not add protocol support. Operations use the
XrdCl protocol implementation and plugins available in the installation. For
example, HTTP and HTTPS access requires the XrdCl HTTP plugin, and support for
an operation such as directory listing still depends on that plugin and the
remote server. Protocols supported by GFAL but without an XrdCl implementation
are not added by this compatibility work.

The following are intentionally outside this first slice:

- a new `xrd` command;
- raw `query` in command-first form;
- uploads, third-party copies, or any other copy with a remote destination;
- exact recursive-copy layout, copy-chain, or dry-run parity;
- full gfal2 common-option parity;
- exact output, diagnostic, or exit-code parity.

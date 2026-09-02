# Using XRootD tools for gfal2-util workflows

This document covers compatibility work for operators moving common read-only
and namespace workflows from gfal2-util to XRootD tools.

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
DIR_A='root://storage.example.org//store/data/new-a/'
DIR_B='root://storage.example.org//store/data/new-b/'
FILE_A='root://storage.example.org//store/data/a.dat'
FILE_B='root://storage.example.org//store/data/b.dat'
```

| gfal2-util | `xrdfs` | Compatibility provided |
| --- | --- | --- |
| `gfal-stat "$FILE_A"` | `xrdfs stat "$FILE_A"` | Stats a complete URL using the existing `xrdfs stat` implementation. |
| — | `xrdfs stat --json "$FILE_A"` | Emits one stable JSON object per operand for compatibility wrappers and other machine consumers. |
| `gfal-ls "$DIR"` | `xrdfs ls "$DIR"` | Lists the contents of the directory. |
| `gfal-ls -lH "$DIR"` | `xrdfs ls -lH "$DIR"` | Accepts the gfal-style `-H` human-readable-size option, including grouped `-lH`. The existing `xrdfs ls -h` spelling remains supported. |
| `gfal-ls -d "$DIR"` | `xrdfs ls -d "$DIR"` | Prints the directory operand itself instead of listing its contents. |
| `gfal-ls -1 "$DIR"` | `xrdfs ls -1 "$DIR"` | Accepts `-1` as a compatibility option for one-entry-per-line formatting. |
| `gfal-ls -a "$DIR"` | `xrdfs ls -a "$DIR"` | Accepts `-a` / `--all` as a compatibility no-op because native `xrdfs ls` already includes dotfiles. |
| `gfal-ls --color=never "$DIR"` | `xrdfs ls --color=never "$DIR"` | Accepts the explicitly uncolored form as a compatibility no-op. |
| `gfal-ls -l --xattr user.status "$FILE_A"` | `xrdfs ls -l --xattr user.status "$FILE_A"` | Appends the requested virtual or native attribute value to long or JSON output. The option is repeatable and preserves order. As in GFAL, it has no visible effect without `-l` unless `--json` is selected. |
| — | `xrdfs ls --json --xattr user.status "$DIR"` | Emits one JSON object per entry, implies stat metadata, and preserves requested attribute order. |
| `gfal-cat -b "$FILE_A"` | `xrdfs cat -b "$FILE_A"` | Accepts `-b` as a compatibility no-op because `xrdfs cat` already writes file data to standard output without text conversion. |
| `gfal-cat -b "$FILE_A" "$FILE_B"` | `xrdfs cat -b "$FILE_A" "$FILE_B"` | Concatenates multiple files from the same endpoint. |
| `gfal-sum "$FILE_A" ADLER32` | `xrdfs sum "$FILE_A" ADLER32` | Selects the requested checksum algorithm and validates that the server returned that algorithm. |
| `gfal-xattr "$FILE_A"` | `xrdfs xattr "$FILE_A"` | Lists the GFAL virtual XRootD attributes available through existing checksum and space queries. |
| `gfal-xattr "$FILE_A" xroot.cksum` | `xrdfs xattr "$FILE_A" xroot.cksum` | Resolves a GFAL virtual attribute through the existing XrdCl checksum query. |
| `gfal-xattr "$FILE_A" user.checksum.adler32` | `xrdfs xattr "$FILE_A" user.checksum.adler32` | Selects the requested checksum algorithm and prints only its digest. |
| `gfal-xattr "$FILE_A" user.status` | `xrdfs xattr "$FILE_A" user.status` | Derives GFAL's disk/tape status from XRootD stat flags or WebDAV Tape REST locality. |

Known GFAL virtual attribute names use a thin adapter implemented with
operations that already exist in XrdCl:

| Virtual attribute | Existing XrdCl operation |
| --- | --- |
| `xroot.cksum` | Default checksum query; returns the algorithm and digest. |
| `user.checksum.<algorithm>` | Checksum query selecting `<algorithm>`; returns the digest. |
| `xroot.space` | Space query. |
| `xroot.xattr` | XRootD xattr query. |
| `spacetoken` | Existing space-info helper, formatted as the GFAL JSON object. |
| `user.status` | XRootD stat query or WebDAV Tape REST archive-info query; both map to `ONLINE`, `NEARLINE`, `ONLINE_AND_NEARLINE`, or `UNKNOWN`. |
| `taperestapi.version` | WebDAV Tape REST discovery API version. |
| `taperestapi.uri` | WebDAV Tape REST discovery endpoint URI. |
| `taperestapi.sitename` | WebDAV Tape REST discovery site name. |

The bare shorthand list is available for XRootD protocols and queries the four
fixed attributes `xroot.cksum`, `xroot.space`, `xroot.xattr`, and `spacetoken`.
`user.checksum.<algorithm>` and `user.status` are requested by name rather than
included in that list. For WebDAV URLs, the bare shorthand list instead exposes
the three `taperestapi.*` discovery attributes above. The checksum shorthand can
also work through another protocol plugin, such as XrdClHttp, when that plugin
supports the checksum query. The remaining `xroot.*` and `spacetoken` virtual
attributes are specific to XRootD protocols.

### Machine-readable metadata

`stat --json` and `ls --json` emit newline-delimited JSON: each successful path
or directory entry is written as one complete object on one line. This permits
multi-path stat and large listings to be consumed incrementally. JSON mode
suppresses the usual labels, spacing, and long-listing columns. For `ls`, it
implies stat metadata; `-d` selects the operand itself, and repeated `--xattr`
requests are returned in order as `{"name": ..., "value": ...}` objects.
Unstatable directory entries are omitted. With `-u`, `path` contains a complete
URL; for file operands, `-C` requests and includes the checksum.

Every object has the same fields: `path`, `type`, `size`, integer `mtime`,
`atime`, and `ctime`, numeric `flags`, ordered `flag_names`, `extended`,
nullable `mode`, `permissions`, `owner`, `group`, and `checksum`, plus the
`xattrs` array. `type` is `file`, `directory`, or `other`. When the server does
not return extended stat data, its mode and ownership fields are `null` and the
unavailable access and change times are zero. `mode` is the raw octal string;
`permissions` is the symbolic nine-character form. All JSON strings are UTF-8.
Invalid byte sequences make the command fail rather than being silently
replaced. Use `--` before a dash-prefixed `stat` path, including a path named
`--json`.

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

## Namespace mutations

The namespace compatibility layer only parses gfal-style operands and then
calls the existing XrdCl `MkDir`, `ChMod`, and `Mv` operations. It does not add
a second namespace implementation.

| gfal2-util | `xrdfs` | Compatibility provided |
| --- | --- | --- |
| `gfal-mkdir -p -m 0755 "$DIR"` | `xrdfs mkdir -p -m 0755 "$DIR"` | Accepts separated octal modes, complete URLs, and the existing `-p` spelling. |
| `gfal-mkdir --parents --mode=0755 "$DIR"` | `xrdfs mkdir --parents --mode=0755 "$DIR"` | Adds the long gfal option spellings; `-m0755` and `--mode 0755` also work. |
| `gfal-mkdir -m 0755 "$DIR_A" "$DIR_B"` | `xrdfs mkdir -m 0755 "$DIR_A" "$DIR_B"` | Validates every supplied mode, the endpoint, and every path before the first mutation, then creates directories in operand order. |
| `gfal-chmod 0750 "$FILE_A"` | `xrdfs chmod 0750 "$FILE_A"` | Adds gfal's octal mode-first order. |
| — | `xrdfs chmod "$FILE_A" rwxr-x---` | Preserves xrdfs's symbolic path-first form. Octal path-first mode remains supported too. |
| `gfal-rename "$FILE_A" "$FILE_B"` | `xrdfs mv "$FILE_A" "$FILE_B"` | Renames a file or directory through the existing XrdCl move operation on one ROOT endpoint. |
| `gfal-rm "$FILE_A" "$FILE_B"` | `xrdfs rm "$FILE_A" "$FILE_B"` | Removes files through the existing XrdCl `Rm` operation. All URL operands must use one endpoint. |
| `gfal-rm -f "$FILE_A"` | `xrdfs rm -f "$FILE_A"` | Accepts `-f` and `--force`; missing files and arguments are ignored. |
| `gfal-rm -r "$DIR"` | `xrdfs rm -r "$DIR"` | Adds `-r`, `-R`, and `--recursive`; native directory trees use an iterative CLI-local postorder traversal. Grouped flags like `-rf` are accepted. |
| `gfal-rm --dry-run "$FILE_A"` | `xrdfs rm --dry-run "$FILE_A"` | Uses XrdCl metadata operations to print tab-separated `<path>` and `SKIP` fields without issuing a removal operation. |
| `gfal-rm -r --dry-run "$DIR"` | `xrdfs rm -r --dry-run "$DIR"` | Lists the tree and prints files before directories; directories use `SKIP DIR`. Remote storage is not changed. |

`mkdir` deliberately retains xrdfs's historical default mode of `0750`.
Scripts that depend on gfal-mkdir's different default should pass an explicit
mode, which makes the intended permissions portable and reviewable.

For `chmod`, path-first interpretation wins when the second operand is a valid
symbolic or octal mode. Otherwise, the first operand must be a valid octal mode
and is interpreted using gfal's mode-first order. Symbolic mode-first input is
rejected, avoiding an ambiguous change to the legacy xrdfs grammar.

`mv` accepts two complete URLs when their protocol, credentials, host, and
effective port identify the same endpoint. It is a namespace rename, not a
cross-storage copy: mixed endpoints are rejected before the remote operation,
while the legacy `xrdfs <endpoint> mv <source> <destination>` form remains
supported.

On the usual local XrdOss backend, an authorized rename replaces an existing
regular-file destination. Authorization policy and other storage backends can
apply different overwrite rules. Directory collisions and exact error or exit
statuses are likewise backend-specific; migration code should depend on the
success or failure of the operation rather than a particular diagnostic.

This rename mapping is currently claimed only for native XRootD protocols.
XrdClHttp does not implement the XrdCl `Mv` operation, so parsing complete
HTTP(S) or DAV(S) operands does not provide a WebDAV rename operation.

These namespace operations use the native XRootD protocol directly. Its server
always grants owner read, write, and execute permissions when creating a
directory, even when fewer owner permissions were requested. Complete HTTPS or
DAVS URLs still require the installed XrdClHttp plugin, and support for
permissions and namespace flags depends on that backend; WebDAV parity is
tracked separately.

### Removal safety and recursive removal

Native XRootD distinguishes file removal from directory removal and lets the
server enforce that `rmdir` only removes an empty directory. WebDAV instead
defines DELETE on a collection as recursive. Before using HTTP(S) or DAV(S)
DELETE, `xrdfs rm` therefore verifies that every operand is a file, and
`xrdfs rmdir` verifies that its operand is an empty directory. Any failed,
partial, or ambiguous metadata response is rejected without sending DELETE.

The checks are deliberately limited to WebDAV-backed protocols. Native ROOT
removal continues to use the existing server operation directly, including its
symlink behavior. The WebDAV checks are client-side preflights rather than an
atomic server primitive, so callers must still exclude concurrent namespace
changes while removing a directory.

With `-r`, `-R`, or `--recursive`, `xrdfs rm` remains a thin client of existing
XrdCl operations rather than adding a recursive filesystem API. For each native
ROOT operand it first tries `Rm`, which removes ordinary files and safely
unlinkable symlinks without a metadata lookup. A directory response is checked
with `RmDir`: an empty directory is removed immediately, while only an explicit
nonempty-directory response permits a non-recursive `DirList`. Children are
then processed by an iterative postorder stack and the directory is removed
with `RmDir`. This avoids call-stack depth limits and never uses
`DirListFlags::Recursive`.

The native probe is important for directory symlinks. Some Linux backends can
report a directory error when `Rm` sees an absolute symlink to a directory;
`RmDir` fails on the symlink itself, so `xrdfs` stops instead of listing and
following its target. Partial listings, unsafe child names, HTTP 207
Multi-Status, and other ambiguous errors are terminal for that tree. Later
top-level operands are still attempted, and the first error is returned.

WebDAV collection DELETE is recursive by protocol definition. A successful
DELETE therefore completes that operand directly. `xrdfs` does not emulate a
second client-side traversal after success, and a 207 Multi-Status remains a
failure because it can describe partially deleted descendants.

Every recursive operand is checked before any mutation. Empty paths, the
namespace root, and dot or dot-dot traversal components (including encoded
forms) are rejected. Child paths retain the operand's URL query parameters.
Use `--` before a dash-prefixed path.

`--dry-run` takes a separate metadata-only path before both the WebDAV safety
guard and the destructive recursive walker. A non-recursive plan calls `Stat`
for each operand. A recursive plan calls `Stat`, lists directories without the
recursive listing flag, and uses an iterative postorder stack so planned files
are reported before their parent directory. It never calls `Rm` or `RmDir`, and
an HTTP(S) or DAV(S) dry-run never sends DELETE. If one tree cannot be fully
inspected, that tree stops, later top-level operands are still planned, and the
first failure is returned. Output uses gfal's `SKIP`, `SKIP DIR`, and `MISSING`
labels where applicable.

Signed query parameters are preserved on every descendant metadata request.
`authz` values are redacted from dry-run output and diagnostics. XrdCl does not
provide an lstat operation, so a metadata-only plan can follow a directory
symlink and should be treated as advisory. The planner fails a tree before it
would exceed 4096 directory levels, so cyclic or hostile listings cannot make
the metadata traversal grow indefinitely. This limit applies only to dry-run;
actual recursive removal continues to use its non-following `Rm`/`RmDir`
checks.

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

## Tape REST and HSM operations

Tape staging, locality queries, cancellation, and eviction use the native XRootD
prepare/query engine and the XrdClHttp WLCG Tape REST client.

| gfal2-util | `xrdfs` | Compatibility provided |
| --- | --- | --- |
| `gfal-bringonline "$FILE"` | `xrdfs prepare -s "$FILE"` | Submits a stage request from tape to disk buffer and prints the Request ID. |
| `gfal-bringonline --pin-lifetime 86400 "$FILE"` | `xrdfs prepare -s --pin-lifetime 86400 "$FILE"` | Passes the requested disk retention lifetime (`diskLifetime`). |
| `gfal-bringonline --staging-metadata '{"example-site":{"queue":"bulk"}}' "$FILE"` | `xrdfs prepare -s --metadata '{"example-site":{"queue":"bulk"}}' "$FILE"` | Passes the site-defined object as Tape REST `targetedMetadata`. |
| `gfal-bringonline --polling-timeout 600 "$FILE"` | `xrdfs prepare -s --wait --timeout 600 "$FILE"` | Polls synchronously until all staged files reach a terminal state (`COMPLETED` or `FAILED`). |
| `gfal-stage-status "$ENDPOINT" "$REQ_ID"` | `xrdfs "$ENDPOINT" query prepare "$REQ_ID"` | Queries the status and per-file state of an existing stage request. |
| `gfal-stage-cancel "$ENDPOINT" "$REQ_ID" "$FILE"` | `xrdfs prepare -a "$REQ_ID" "$FILE"` | Cancels/aborts a stage request for the specified complete URLs. |
| `gfal-evict "$FILE" "$REQ_ID"` | `xrdfs prepare -e "$REQ_ID" "$FILE"` | Releases complete-URL disk-pinned files back to tape-only. |
| `gfal-archivepoll "$FILE"` | `xrdfs "$ENDPOINT" query tape archiveinfo "$FILE"` | Queries archive locality information (`ONLINE`, `NEARLINE`, `ONLINE_AND_NEARLINE`). |
| `gfal-xattr "$FILE" user.status` | `xrdfs xattr "$FILE" user.status` | Maps Tape REST locality to GFAL's `ONLINE`, `NEARLINE`, or `ONLINE_AND_NEARLINE` values for WebDAV URLs. |
| `gfal-xattr "$FILE"` | `xrdfs xattr "$FILE"` | Lists `taperestapi.version`, `taperestapi.uri`, and `taperestapi.sitename` discovered from a WebDAV endpoint. |
| — | `xrdfs "$ENDPOINT" query tape discover` | Discovers WLCG Tape REST API endpoint version, site name, and base URI. |
| — | `xrdfs "$ENDPOINT" query prepare -d "$REQ_ID"` | Deletes a stage request record from the tape REST server. |

## Testing approach

The XRootD tests exercise the compatibility behavior against a local XRootD
server and controlled fixtures. Namespace tests mutate only that ephemeral
localhost fixture. They cover:

- complete-URL parsing and preservation of URL parameters;
- legacy server-first syntax;
- `stat`;
- normal, long human-readable, and directory-entry `ls`;
- binary-safe and multiple-file `cat`;
- algorithm-selecting checksum queries through `sum`;
- GFAL virtual xattr shorthand and explicit native xattr list/get;
- remote-to-local `xrdcp` downloads, including overwrite, checksum validation,
  input lists, and stdout;
- rejection of local URLs and mixed remote endpoints;
- octal and symbolic namespace modes, all supported `mkdir` option spellings,
  multiple directory operands, legacy syntax, and pre-mutation validation;
- same-endpoint ROOT renames through complete-URL and legacy syntax, including
  regular-file replacement, directory trees, failure preservation, and
  mixed-endpoint prevalidation;
- fail-closed WebDAV non-recursive removal decisions;
- recursive native ROOT trees, empty directories, multiple operands, missing
  targets, special names, root and traversal guards, deep trees, and directory
  symlink target preservation;
- successful WebDAV collection DELETE and terminal multi-status handling;
- metadata-only file and recursive removal plans, including postorder output,
  continuation after a missing operand, signed-query preservation, authz
  redaction, and zero native or WebDAV removal requests;

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

The enabled suite starts its own localhost XRootD server and uses generated
test data. Namespace mutations and copy destinations remain below the test
temporary directory.
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

The default output remains native `xrdfs` output. In particular, human-readable
`stat`, long `ls`, and xattr formatting may differ from gfal2-util in field
names, ordering, path presentation, size rounding, timestamps, and diagnostics.
Scripts that parse gfal2-util output should use the explicit JSON mode as their
metadata interface rather than assuming identical human-readable text.

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

In particular, XrdClHttp does not currently implement `Mv`; HTTPS and DAV
rename compatibility is not claimed here.

The following are intentionally outside the compatibility covered here:

- a new `xrd` command;
- raw `query` in command-first form;
- gfal-rm auxiliary modes (`--just-delete`, `--from-file`, and `--bulk`);
- uploads, third-party copies, or any other copy with a remote destination;
- exact recursive-copy layout, copy-chain, or `gfal-copy` dry-run parity;
- full gfal2 common-option parity;
- exact output, diagnostic, or exit-code parity.

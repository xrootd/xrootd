# XrdSecssh

`XrdSecssh` is an experimental XRootD security protocol plugin (`sec.protocol ssh`)
for SSH-key-based authentication over TLS.

## V1 behavior

- Raw key mode and OpenSSH user certificate mode
- Server trust source:
  - `keys-file` for raw key -> user mapping
  - optional `ca-keys-file` for user certificate validation
- Supported raw key algorithms: `ssh-ed25519`, `ssh-rsa`
- Supported user certificate algorithms: `ssh-ed25519-cert-v01@openssh.com`,
  `ssh-rsa-cert-v01@openssh.com`
- Two round-trips (four messages):
  1. client sends user + SSH key/certificate blob
  2. server sends nonce challenge
  3. client signs challenge with private key
  4. server verifies signature and maps to local username

## Server configuration

With no trailing parameters, `sec.protocol ssh` uses these defaults:

- `-keys-file /etc/xrootd/ssh_authorized_keys`
- `-maxsz 8192`
- `-nonce-ttl 30`

At least one trust source must be configured at init time: either `keys-file`
(with at least one usable raw key) or `ca-keys-file` (with at least one CA key).
Certificate-only deployments may omit or leave `keys-file` empty when
`-ca-keys-file` is set.

```conf
sec.protocol ssh \
  -keys-file /etc/xrootd/ssh_authorized_keys \
  -ca-keys-file /etc/xrootd/ssh_ca_keys \
  -principal-as-user \
  -principal-map \
  -maxsz 8192 \
  -nonce-ttl 30 \
  -debug
```

Certificate-only example (no raw keys):

```conf
sec.protocol ssh \
  -keys-file /etc/xrootd/ssh_authorized_keys \
  -ca-keys-file /etc/xrootd/ssh_ca_keys \
  -principal-map
```

When `-ca-keys-file` is configured, a missing or empty `keys-file` is accepted
at init time as long as the CA file loads successfully.

`keys-file` security checks:

- opened with `O_NOFOLLOW` (symlinks are rejected)
- must be a regular file
- must be owned by the effective xrootd uid
- must not be group/other writable
- must not exceed 10 MB

The same security checks apply to `ca-keys-file` when configured.
The same security checks apply to `principal-map-file` when configured.

> **Note:** the ownership check requires the file to be owned by the *effective*
> uid the `xrootd` process runs as. A `root`-owned key file will be rejected when
> `xrootd` runs as an unprivileged service account; make the key files owned by
> that account.

`-keys-file` and `-ca-keys-file` are read once at plugin initialization, so a
restart is required to pick up changes. Only the `principal-map-file` is
hot-reloaded (see below).

Validated option ranges:

- `-maxsz <bytes>`: `1`..`524288` (default `8192`)
- `-nonce-ttl <seconds>`: `1`..`600` (default `30`)

### keys-file format

Accepted line formats:

1. Explicit user mapping:

```text
foo ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI...
foo ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ...
```

2. Authorized-keys style fallback mapping (username extracted from comment prefix before `@`):

```text
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... foo@host
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ... foo@host
```

### ca-keys-file format (optional)

Each line should contain a CA public key:

```text
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI...
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ...
```

Also accepted:

```text
cert-authority ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI...
```

When a user certificate is presented, the server validates:

- certificate signature against a trusted CA key
- certificate type is user (`type=1`)
- validity window (`valid_after` / `valid_before`)
- certificate carries no critical options (any critical option is rejected,
  i.e. the server fails closed on options such as `force-command` or
  `source-address` that it does not enforce)
- principals contain requested user (if principals list is non-empty)

> **Security note:** following OpenSSH semantics, a certificate with an *empty*
> principals list is treated as valid for any requested user when neither
> `-principal-as-user` nor `-principal-map` is enabled. Because the CA is fully
> trusted, only issue zero-principal user certificates if every holder is meant
> to authenticate as an arbitrary account.
>
> When `-principal-as-user` or `-principal-map` is enabled, an empty principals
> list is rejected (mapping requires at least one principal to resolve).

### Principal mapping options (cert mode)

Server options:

- `-principal-as-user`:
  map a certificate principal directly to local account if it is a valid
  local username or uid.
- `-principal-map`:
  enable principal mapping file at default path (no argument)
  `/etc/xrootd/ssh_principals.map`.
- `-principal-map-file <path>`:
  use a custom principal mapping file.

If both direct and file mapping are enabled, direct principal->local-user
mapping is tried first, then the map file.

Map file format:

```text
principal-a alice
principal-b 1001
```

Each line is `<principal> <username|uid>`.

The principal map file is monitored during authentication. On each auth the
plugin first checks inode/mtime with a stat-only probe; the file is read and
parsed only when it changed.

## Client configuration

The client receives `TLS:<version>:<maxsz>:` from the server init token. The
client-side `sec.protocol ssh` parameters use the same `maxsz` bound for
credential size.

> **Certificate clients:** private key *file* mode only supports raw
> `ssh-ed25519` / `ssh-rsa` keys. To authenticate with an OpenSSH user
> certificate, the client must use `ssh-agent` mode (the certificate identity is
> selected from the agent).

Default mode uses a private key file (PEM/PKCS8 format; supported key types: ed25519, rsa):

```sh
export XRD_SSH_KEY_FILE=/path/to/ed25519-private.pem
```

`XRD_SSH_PRIVATE_KEY_FILE` is accepted as an alias and is consulted when
`XRD_SSH_KEY_FILE` is unset.

Client `ssh-agent` mode is also supported:

```sh
export SSH_AUTH_SOCK=/run/user/1000/ssh-agent.socket
export XRD_SSH_AGENT=1
```

When `XRD_SSH_AGENT=1`, the client picks a supported identity from the agent
and signs the server challenge via agent.

Supported agent identities:

- raw keys: `ssh-ed25519`, `ssh-rsa`
- user certificates: `ssh-ed25519-cert-v01@openssh.com`,
  `ssh-rsa-cert-v01@openssh.com`

Optional key selection by fingerprint:

```sh
export XRD_SSH_AGENT_FINGERPRINT='SHA256:base64fingerprint'
```

Fallback behavior:

- if `XRD_SSH_AGENT` is not set, key-file mode is tried first
- if key-file is not configured and `SSH_AUTH_SOCK` exists, agent mode is used
- if not in agent mode, no key-file env is set, and `XRD_SSH_USER` is not set,
  the client also tries default key files in order:
  - `~/.ssh/id_ed25519`
  - `~/.ssh/id_rsa`
- if all methods fail, authentication fails with a detailed error

Optional username override:

```sh
export XRD_SSH_USER=foo
```

If `XRD_SSH_USER` is not set, `USER` is used.

## Quickstart

Minimal server (`/etc/xrootd/xrootd.cf`):

```conf
xrootd.seclib libXrdSec.so
all.role server
sec.protocol ssh -keys-file /etc/xrootd/ssh_authorized_keys
xrootd.tls all
xrd.tlsca certdir /etc/grid-security/certificates
xrd.tls /etc/grid-security/xrd/xrdcert.pem /etc/grid-security/xrd/xrdkey.pem
```

Add a trusted raw key to `/etc/xrootd/ssh_authorized_keys` (owned by the xrootd
uid, mode `0600`):

```text
alice ssh-ed25519 AAAA...
```

Client access with a PEM private key:

```sh
export XRD_SSH_KEY_FILE="$HOME/.ssh/id_ed25519.pem"
export XRD_SSH_USER=alice
xrdfs -s root://localhost:1094/ ls /
```

## Notes

- TLS is mandatory (`needTLS() == true`)
- handshake nonce is single-use and expires after `-nonce-ttl`
- debug logging can be enabled via `-debug` or `XrdSecDEBUG=1`; it prints
  loaded key metadata (alg/user/fingerprint) and authentication key selection
- RSA signatures (challenge responses and certificate signatures) use
  `rsa-sha2-256`. Legacy `ssh-rsa` (SHA-1) signatures are not accepted, so RSA
  certificates must be signed by a CA using `rsa-sha2-256`.
- Supported key algorithms: `ssh-ed25519`, `ssh-rsa` (and matching user-cert
  variants). ECDSA, FIDO/`sk-*`, and other OpenSSH key types are not supported in
  V1.

## Security considerations

- This protocol authenticates the *client* to the server. It does not
  authenticate the server within the handshake and provides no channel binding
  between the SSH challenge/response and the underlying TLS session. Its safety
  therefore relies on TLS with server-certificate verification to prevent
  man-in-the-middle relay of the signed challenge; do not disable TLS peer
  verification.
- The challenge is signed over `xrdsec-ssh-v1|<nonce>|<fingerprint>`, binding the
  response to the presented key/certificate and the single-use server nonce.
- Only one pending challenge is allowed per transport identity (`tident`); a
  second init on the same connection is rejected with `EBUSY`.
- SSH wire string fields are capped at 64 KiB; `keys-file` lines and base64 key
  material have separate limits to reduce parser DoS risk at init.
- Client private key files must be owned by the effective uid and must not be
  group/other accessible; `ssh-agent` sockets must be owned by the effective uid
  and must not be group/other accessible.
- Mapped usernames are restricted to a conservative charset and length before
  being stored in `XrdSecEntity.name`.
- Debug logging (`-debug` / `XrdSecDEBUG=1`) prints redacted key fingerprints
  and omits transport usernames and socket paths.
- The principal map file is reloaded under a mutex so lookups are not racy with
  hot reload.
- In certificate mode the CA is fully trusted: any principal it issues is
  accepted (subject to the validity window, `type=1`, and the no-critical-options
  rule). Restrict the CA key set in `ca-keys-file` accordingly.

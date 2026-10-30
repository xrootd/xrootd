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
  3. client signs `nonce + key fingerprint + hostname it connected to`
  4. server checks the hostname is one of its own, verifies the signature and
     maps to a local username

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
  -revoked-keys-file /etc/xrootd/ssh_revoked_keys \
  -principal-as-user \
  -principal-map \
  -hostnames data1.example.org,storage.example.org \
  -deny-users root,daemon \
  -maxsz 8192 \
  -nonce-ttl 30 \
  -debug
```

All options:

| option | default | meaning |
|---|---|---|
| `-keys-file <path>` | `/etc/xrootd/ssh_authorized_keys` | raw key -> user mapping (read at init) |
| `-ca-keys-file <path>` | unset | trusted CA public keys for user certificates (read at init) |
| `-revoked-keys-file <path>` | unset | revoked keys / certificates (hot-reloaded) |
| `-principal-as-user` | off | map a certificate principal directly to a local account |
| `-principal-map` / `-principal-map-file <path>` | unset | principal -> user map (hot-reloaded) |
| `-allow-empty-principals` | off | accept certificates with an empty principals list for any user |
| `-deny-users <a,b,...>` \| `none` | `root` | local accounts that may never be the result of a mapping |
| `-hostnames <a,b,...>` | see below | additional names/IPs under which clients may address this server |
| `-maxsz <bytes>` | `8192` | maximum credential size (`1`..`524288`) |
| `-nonce-ttl <seconds>` | `30` | challenge lifetime (`1`..`600`) |
| `-debug` | off | verbose logging |

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

The same security checks apply to `ca-keys-file`, `revoked-keys-file` and
`principal-map-file` when configured.

Lines in `keys-file` / `ca-keys-file` that cannot be used (unsupported key
type, bad base64, RSA modulus below 2048 bits) are skipped with a warning in the
server log; a fingerprint that appears twice also logs a warning (the later
mapping wins).

> **Note:** the ownership check requires the file to be owned by the *effective*
> uid the `xrootd` process runs as. A `root`-owned key file will be rejected when
> `xrootd` runs as an unprivileged service account; make the key files owned by
> that account.

`-keys-file` and `-ca-keys-file` are read once at plugin initialization, so a
restart is required to pick up changes. `principal-map-file` and
`revoked-keys-file` are hot-reloaded (see below).

### Hostname binding

The client signs the hostname it connected to (as given in the `root://` URL or
redirect) and sends it with the response. The server accepts a response only if
that name is one of its own: the canonical FQDN and the kernel hostname (long
and short). Names are compared case-insensitively without a trailing dot.

Loopback names (`localhost`, `localhost.localdomain`, `127.0.0.1`, `::1`) are
not accepted unless listed in `-hostnames`. They are not part of the default
set, because every server would accept a signature bound to them.

If clients reach the server through an alias, a load-balancer name, an IP
literal, or loopback, list those with `-hostnames`; otherwise authentication
fails with "SSH challenge is bound to a different server" and the server log
shows the name the client used. Servers that share an alias both accept
signatures bound to that name, so a signature can still be relayed between
them.

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
- validity window (`valid_after` <= now < `valid_before`, as in OpenSSH;
  `valid_before = 0` is already expired)
- certificate carries no critical options (any critical option is rejected,
  i.e. the server fails closed on options such as `force-command` or
  `source-address` that it does not enforce)
- principals contain the requested user when principal mapping is disabled and the principals list is non-empty
- the certificate, its subject key, its signing CA, serial and key id are not revoked
- RSA subject keys are at least 2048 bits. An RSA CA signature must be
  `rsa-sha2-256` or `rsa-sha2-512`. The legacy SHA-1 `ssh-rsa` label is
  rejected. Challenge responses stay `rsa-sha2-256`.

> **Empty principals:** OpenSSH treats a certificate with an *empty* principals
> list as valid for any user. This plugin rejects such certificates by default.
> Set `-allow-empty-principals` to restore the OpenSSH behaviour; even then the
> `-deny-users` list (default `root`) still applies. When `-principal-as-user`
> or `-principal-map` is enabled an empty principals list is always rejected.

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
mapping is tried first, then the map file. When the client requested a specific
user, a principal that maps to that user is preferred over the first mappable
principal, so a certificate listing `alice` and `bob` can be used as either.

Map file format:

```text
principal-a alice
principal-b 1001
```

Each line is `<principal> <username|uid>`.

The principal map file is monitored during authentication. On each auth the
plugin first checks inode, size and modification time (nanoseconds where the
platform provides them) with a stat-only probe; the file is read and parsed
only when one of those changed. An in-place rewrite is picked up when
that stamp changes. If the inode, size and timestamp all stay the same,
down to the resolution the filesystem records, the previous contents stay
in use. Replace the file by writing a new one and renaming it into place
so the inode changes.

### revoked-keys-file format (optional)

Hot-reloaded like the principal map. One entry per line:

```text
# a public key: raw key, certificate subject key, or signing CA
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... alice@laptop
# by fingerprint of a key, a certificate, or a CA
SHA256:base64fingerprint
# certificate serial number / key id
serial: 42
id: alice-2026-01
```

Binary OpenSSH KRL files are not supported; use `ssh-keygen -L` to obtain the
serial and key id of a certificate to revoke.

### deny-users

`-deny-users` lists local accounts that may never be the result of any mapping
(raw key, certificate principal, principal map). The default is `root`; pass
`-deny-users none` to disable the list.

## Client configuration

The client receives `TLS:<version>:<maxsz>:` from the server init token. The
client-side `sec.protocol ssh` parameters use the same `maxsz` bound for
credential size.

> **Certificate clients:** private key *file* mode only supports raw
> `ssh-ed25519` / `ssh-rsa` keys. To authenticate with an OpenSSH user
> certificate, the client must use `ssh-agent` mode (the certificate identity is
> selected from the agent).

Default mode uses a private key file. Both the OpenSSH native format written by
`ssh-keygen` (`-----BEGIN OPENSSH PRIVATE KEY-----`) and PEM/PKCS8 are accepted
for ed25519 and rsa keys; RSA keys must be at least 2048 bits. Passphrase
protected keys are refused (the client never prompts) -- use `ssh-agent` for
those.

```sh
export XRD_SSH_KEY_FILE=$HOME/.ssh/id_ed25519
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

- if `XRD_SSH_KEY_FILE` is set and `XRD_SSH_AGENT` is not, that key file is used
- otherwise, if `SSH_AUTH_SOCK` is set, the client tries the agent
- `XRD_SSH_AGENT=1` stops there: a failed agent is not followed by a key file
- otherwise, after an agent with no usable identity (or when no agent socket
  is set), the client tries:
  - `~/.ssh/id_ed25519`
  - `~/.ssh/id_rsa`
- `XRD_SSH_USER` changes only the account name. It does not skip those files
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
sec.protocol ssh -keys-file /etc/xrootd/ssh_authorized_keys -hostnames localhost
xrootd.tls all
xrd.tlsca certdir /etc/grid-security/certificates
xrd.tls /etc/grid-security/xrd/xrdcert.pem /etc/grid-security/xrd/xrdkey.pem
```

Add a trusted raw key to `/etc/xrootd/ssh_authorized_keys` (owned by the xrootd
uid, mode `0600`):

```text
alice ssh-ed25519 AAAA...
```

Client access with the key `ssh-keygen -t ed25519` produced:

```sh
export XRD_SSH_KEY_FILE="$HOME/.ssh/id_ed25519"
export XRD_SSH_USER=alice
xrdfs -s root://localhost:1094/ ls /
```

## Notes

- TLS is mandatory (`needTLS() == true`)
- handshake nonce is single-use and expires after `-nonce-ttl`
- debug logging can be enabled via `-debug` or `XrdSecDEBUG=1`; it prints
  loaded key metadata (alg/user/fingerprint) and authentication key selection
- Challenge responses use `rsa-sha2-256`. An RSA CA may sign a user
  certificate with `rsa-sha2-256` or `rsa-sha2-512` (the `ssh-keygen -s`
  default). The legacy SHA-1 `ssh-rsa` label is not accepted. RSA keys below
  2048 bits are rejected everywhere.
- `ssh-agent` I/O is bounded by a 10 s timeout.
- Supported key algorithms: `ssh-ed25519`, `ssh-rsa` (and matching user-cert
  variants). ECDSA, FIDO/`sk-*`, and other OpenSSH key types are not supported in
  V1.

## Security considerations

- This protocol authenticates the *client* to the server; server
  authentication and confidentiality come from TLS, so do not disable TLS peer
  verification.
- The signed payload is `"xrdsec-ssh-v2" || string(host) || string(nonce) ||
  string(fingerprint)` where `host` is the name the client connected to. A
  server that receives a response bound to a name it is not known under rejects
  it, so a rogue or compromised server (for example one reached through a
  redirect) cannot relay a client's signature to another server in the
  federation. This holds only for a name the target server does not accept.
  Servers that share a `-hostnames` alias accept each other's responses for
  that name. Loopback names are not accepted unless an administrator lists
  them in `-hostnames`. This is not full TLS channel binding, but it closes
  the relay path for names a server is uniquely known under, while keeping
  the plugin independent of the TLS implementation.
- Challenge state is kept inside the per-connection protocol object: it is
  single-use, expires after `-nonce-ttl`, disappears when the connection
  closes, and cannot be exhausted or consumed by other connections. A new init
  on the same connection simply replaces the outstanding challenge.
- Clients receive a generic "SSH authentication failed." for an untrusted key,
  a key/username mismatch, an untrusted CA, or a certificate signature that
  does not verify. The specific reason is written to the server log only.
  The certificate fingerprint and the mapped account are included when they
  are already known.
- `-deny-users` (default `root`) is enforced after every mapping path;
  certificates without principals are rejected unless `-allow-empty-principals`
  is set.
- Client private keys are read from the same descriptor that passed the
  ownership/permission checks (no re-open by path).
- SSH wire string fields are capped at 64 KiB; `keys-file` lines and base64 key
  material have separate limits to reduce parser DoS risk at init.
- Client private key files must be owned by the effective uid and must not be
  group/other accessible; `ssh-agent` sockets must be owned by the effective uid
  and must not be group/other accessible.
- Mapped usernames are restricted to a conservative charset and length before
  being stored in `XrdSecEntity.name`.
- Debug logging (`-debug` / `XrdSecDEBUG=1`) prints redacted key fingerprints
  and omits transport usernames and socket paths.
- The principal map and revocation files are reloaded under their own mutexes;
  NSS (`getpwnam`) lookups are performed outside those locks and no global lock
  is held on the authentication path.
- In certificate mode the CA is trusted to certify principals. A certificate
  it issues is still subject to the validity window, `type=1`, the
  no-critical-options rule, the signature algorithm, revocation, principal
  matching, the empty-principal rule, and `-deny-users`. Restrict the CA key
  set in `ca-keys-file` accordingly.

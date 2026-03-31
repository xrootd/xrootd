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
- Two-step handshake:
  1. client sends user + SSH key/certificate blob
  2. server sends nonce challenge
  3. client signs challenge with private key
  4. server verifies signature and maps to local username

## Server configuration

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

`keys-file` security checks:

- must be a regular file
- must be owned by effective xrootd uid
- must not be group/other writable

The same security checks apply to `ca-keys-file` when configured.
The same security checks apply to `principal-map-file` when configured.

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

- certificate signature against trusted CA key
- certificate type is user (`type=1`)
- validity window (`valid_after` / `valid_before`)
- principals contain requested user (if principals list is non-empty)

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

The principal map file is monitored during authentication and is reloaded
automatically if its inode or mtime changes.

## Client configuration

Default mode uses a private key file (PEM/PKCS8 format; supported key types: ed25519, rsa):

```sh
export XRD_SSH_KEY_FILE=/path/to/ed25519-private.pem
```

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

## Notes

- TLS is mandatory (`needTLS() == true`)
- handshake nonce is single-use and expires after `-nonce-ttl`
- debug logging can be enabled via `-debug` or `XrdSecDEBUG=1`; it prints
  loaded key metadata (alg/user/fingerprint) and authentication key selection

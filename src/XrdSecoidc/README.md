# XrdSecOIDC

`XrdSecOIDC` is an XRootD security protocol plugin (`sec.protocol oidc`) that
authenticates bearer-style OIDC tokens over TLS.

This implementation performs OIDC-style JWT validation directly in the OIDC
plugin (no SciTokens helper dependency).

## Client token pickup order

The client side `getCredentials()` searches for a token in this order:

1. `XRD_SSO_TOKEN` (raw token value)
2. `XRD_SSO_TOKEN_FILE` (path to token file)
3. `BEARER_TOKEN` (raw token value, compatibility fallback)
4. `BEARER_TOKEN_FILE` (path to token file, compatibility fallback)
5. `XDG_RUNTIME_DIR/bt_u<uid>`
6. `/tmp/bt_u<uid>`

If no token is found, authentication fails with `ENOPROTOOPT`.

When a token file is used (`*_TOKEN_FILE`, `XDG_RUNTIME_DIR/bt_u<uid>`,
`/tmp/bt_u<uid>`), it must be:

- a regular file (no device/FIFO/etc.),
- owned by the effective client uid,
- accessible by owner only (no group/other permission bits).

## Server initialization parameters

`XrdSecProtocoloidcInit()` supports:

- `-maxsz <num>`: maximum token size (default 8192, max 524288)
- `-expiry {ignore|optional|required}`:
  - `ignore` = do not enforce expiry claim
  - `optional` = enforce only if expiry present
  - `required` = expiry must be present and valid
- `-issuer <url>`: expected `iss` claim value; if `-oidc-config-url` is not
  specified, discovery URL defaults to `<issuer>/.well-known/openid-configuration`
- `-audience <value>`: expected token audience (`aud` string or array member);
  may be repeated for the current issuer
- `-oidc-config-url <https-url>`: OpenID discovery URL (used to locate JWKS URI)
- `-jwks-url <https-url>`: explicit JWKS endpoint (overrides discovery lookup)
- `-jwks-refresh <seconds>`: JWKS refresh cache interval (default 300)
- `-jwks-cache-file <path>`: optional on-disk JWKS cache file shared across
  issuers (disabled by default)
- `-jwks-cache-ttl <seconds>`: TTL for on-disk cached issuer keys
  (`0` = use `-jwks-refresh`; default 0)
- `-clock-skew <seconds>`: allowed clock skew for time-based claims (default 60)
- `-identity-claim <claim>`: add claim names (in order) used for user identity;
  may be specified multiple times
- `-forced-identity-claim <claim>`: for the current `-issuer`, force identity
  extraction to this single claim (no fallback order for that issuer)
  - special case: when set to `email`, the email value is mapped to local
    username via `[email-map]` entries in `/etc/xrootd/oidc.cfg`
- `-debug-token`: print decoded JWT header/payload on successful auth (debug only;
  contains sensitive information)
- `-show-token-claims`: print selected claims only (`alg`, `kid`, `typ`, `iss`,
  `aud`, `sub`, `preferred_username`, `azp`, `iat`, `nbf`, `exp`)
- `-token-cache-max <num>`: max number of cached validated tokens (default 10000;
  set `0` to disable caching)
- `-token-cache-noexp-ttl <seconds>`: cache TTL for tokens that do not include
  `exp` when `-expiry optional|ignore` (default 60)
- `-config-file <path>`: load INI config from this path (instead of default
  `/etc/xrootd/oidc.cfg`); supports runtime reload of `[issuer ...]` and
  `[email-map]` sections on inode/mtime changes

If no inline parameters are supplied on `sec.protocol oidc`, the plugin
automatically tries to load `/etc/xrootd/oidc.cfg` (INI-style) and maps keys to
the same options listed above.

`-issuer` starts a new issuer policy block. `-audience`, `-oidc-config-url`, and
`-jwks-url` that follow apply to that issuer until the next `-issuer`.
`-forced-identity-claim` is also issuer-scoped.

## TLS requirement

`oidc` rejects non-TLS connections. Both client and server constructors enforce
TLS-only use.

## Server-side identity mapping

After signature and claim validation, the server sets `XrdSecEntity.name` from
claims in this default order:

1. `preferred_username`
2. `upn`
3. `username`
4. `name`
5. `sub`

Use repeated `-identity-claim` options to override this order.

Per-issuer override: use `-forced-identity-claim <claim>` after a specific
`-issuer` to force a single claim for that issuer.
If `forced-identity-claim = email` is used, authentication fails unless the
token has an `email` claim and that email is present in `[email-map]`.

## Example config snippet

```conf
sec.protocol oidc \
  -issuer https://issuer-a.example \
  -audience xrootd \
  -audience xrootd-admin \
  -issuer https://issuer-b.example \
  -audience service-b \
  -expiry required
```

## Standard CERN SSO configuration

Inline `xrootd.cf` style:

```conf
sec.protocol oidc \
  -issuer https://auth.cern.ch/auth/realms/cern \
  -audience public-client \
  -expiry required \
  -show-token-claims
```

INI fallback (`/etc/xrootd/oidc.cfg`) equivalent:

```ini
[global]
expiry = required
show-token-claims = true
# issuer configuration
```

## Standard CERN configuration
Example `oidc.cfg` using CERN issuer and default mapping:
```ini
[issuer "https://auth.cern.ch/auth/realms/cern"]
audience = public-client
```

## Standard Google configuration

Example `oidc.cfg` using Google issuer + email mapping:

```ini
[issuer "https://accounts.google.com"]
audience = 780271439668-1aukl5va8p6rbf5i81sgpg5ppr8s2p63.apps.googleusercontent.com
forced-identity-claim = email

[email-map]
foo.bar@gmail.com = foo
```

Google app setup (Google Cloud Console):

1. Create/select a project.
2. Configure OAuth consent screen.
3. Create OAuth client credentials:
   - for `xrdsso ... GOOGLE` default flow (`--flow device`), use a client type
     that supports device authorization and provide both `client_id` and
     `client_secret` to `xrdsso`.
   - for `--flow pkce`, use a Desktop app client id.
4. Copy client id/secret and use them in `xrdsso` options (or env vars).

Quickstart (Google):

```sh
# 0) Minimal server config (example: /etc/xrootd/xrootd.cf)
cat > /etc/xrootd/xrootd.cf <<'EOF'
###########################################################
xrootd.seclib libXrdSec.so
all.role server
sec.protocol oidc
xrootd.tls all
xrd.tlsca certdir /etc/grid-security/certificates
xrd.tls /etc/grid-security/xrd/xrdcert.pem /etc/grid-security/xrd/xrdkey.pem
EOF

# 0b) Start the server (foreground example)
xrootd -c /etc/xrootd/xrootd.cf -R xrootd

# 1) Create a token (Google device flow)
./utils/xrdsso create ./build/google.token GOOGLE \
  --client-id "<google-oauth-client-id>" \
  --client-secret "<google-oauth-client-secret>"

# 2) Inspect token claims
./utils/xrdsso show ./build/google.token

# 3) Try access with xrdfs
XRD_SSO_TOKEN_FILE="$PWD/build/google.token" \
LD_LIBRARY_PATH=/usr/local/lib64 \
/usr/local/bin/xrdfs localhost:2000 stat /tmp/
```

## INI fallback file (`/etc/xrootd/oidc.cfg`)

When `sec.protocol oidc` has no trailing parameters, this file is required and
loaded at plugin init time. If the file is missing, initialization fails.

You can override this path with:

```conf
sec.protocol oidc -config-file /path/to/oidc.cfg
```

When file-backed config is used (default path or `-config-file`), the plugin
checks inode/mtime changes at authentication time and reloads only:

- `[issuer "..."]` blocks (issuer/audience/OIDC+JWKS URLs/forced-identity-claim)
- `[email-map]`

`[global]` keys are intentionally **not** reloaded and remain fixed from startup.

```ini
[global]
maxsz = 8192
expiry = required
jwks-refresh = 300
jwks-cache-file = /var/lib/xrootd/oidc-jwks-cache.ini
jwks-cache-ttl = 600
clock-skew = 60
identity-claim = preferred_username,sub
show-token-claims = true
token-cache-max = 10000
token-cache-noexp-ttl = 60

[issuer "https://issuer-a.example"]
audience = xrootd,xrootd-admin
forced-identity-claim = preferred_username
# Optional if you want to override discovery default:
# oidc-config-url = https://issuer-a.example/.well-known/openid-configuration
# jwks-url = https://issuer-a.example/protocol/openid-connect/certs

[issuer "https://issuer-b.example"]
audience = service-b

[email-map]
alice@example.org = alice
bob@example.org = bobby
```

Supported sections are `[global]` and `[issuer "<issuer-url>"]` (or `[issuer]`
with `issuer = <url>` inside the section), plus `[email-map]` for
`forced-identity-claim = email`. Boolean keys accept `true/false`, `yes/no`,
`on/off`, `1/0`.

Security checks for `/etc/xrootd/oidc.cfg`:

- must be a regular file,
- must be owned by the effective uid of the running xrootd process,
- must not be writable by group or others.

When `jwks-cache-file` is configured, cached keys are stored per issuer in that
file and reused across refresh/restart cycles until TTL expiry.

## Notes

- Protocol id on the wire is `oidc`.
- Accepted JWT signature algorithm is currently `RS256`.
- OIDC discovery and JWKS endpoints must use `https://`.
- Server-side token cache is enabled by default and keyed by raw token value.

## Local helper scripts

For quick manual testing, helper scripts are available in `utils/`:

- `utils/xrdsso create [tokenfile] [CERN|CERNOIDC|GOOGLE]`: creates token
  (`CERN` uses OAuth2 access token flow, `CERNOIDC` requests OIDC scopes and
  requires/stores `id_token`, `GOOGLE` defaults to device flow).
- Shortcut: `utils/xrdsso CERNOIDC [tokenfile]` is equivalent to
  `utils/xrdsso create [tokenfile] CERNOIDC`.
- `utils/xrdsso show [tokenfile]`: decodes and prints JWT header/payload.
- If `<tokenfile>` is omitted, default is `${XDG_RUNTIME_DIR}/bt_u<uid>`
  (or `/tmp/bt_u<uid>` when `XDG_RUNTIME_DIR` is unset).
- For `GOOGLE`, provide `--client-id` (or set `GOOGLE_OAUTH_CLIENT_ID`).
- For `GOOGLE` default device flow, also provide `--client-secret` (or set
  `GOOGLE_OAUTH_CLIENT_SECRET`).
- `GOOGLE` flow can be selected via `--flow device|pkce` (`device` default).
- `--client-secret` is optional for PKCE but may be required for some client
  types/endpoints; env fallback: `GOOGLE_OAUTH_CLIENT_SECRET`.
- For `GOOGLE`, `xrdsso` stores `id_token` when present (JWT), otherwise
  falls back to `access_token`.
- Device flow prints a prefilled verification URL when possible; if your browser
  still asks, use the printed device code.

Example:

```sh
./utils/xrdsso create
./utils/xrdsso CERNOIDC
./utils/xrdsso create GOOGLE \
  --client-id "<google-oauth-client-id>" \
  --client-secret "<google-oauth-client-secret>"
./utils/xrdsso create GOOGLE --flow pkce \
  --client-id "<google-oauth-client-id>"
./utils/xrdsso show 
```

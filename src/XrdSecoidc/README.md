# XrdSecOIDC

`XrdSecOIDC` is an XRootD security protocol plugin (`sec.protocol oidc`) that
authenticates bearer-style OIDC tokens over TLS.

This implementation performs OIDC-style JWT validation directly in the OIDC
plugin (no SciTokens helper dependency).

## Client token pickup order

The client side `getCredentials()` searches for a token in this order:

1. `BEARER_TOKEN` (raw token value)
2. `BEARER_TOKEN_FILE` (path to token file)
3. `XDG_RUNTIME_DIR/bt_u<uid>`
4. `/tmp/bt_u<uid>`

If no token is found, authentication fails with `ENOPROTOOPT`.

When a token file is used (`*_TOKEN_FILE`, `XDG_RUNTIME_DIR/bt_u<uid>`,
`/tmp/bt_u<uid>`), it must be:

- a regular file (no device/FIFO/etc.),
- owned by the effective client uid,
- accessible by owner only (no group/other permission bits),
- opened with `O_NOFOLLOW` (a symlink at the token path is rejected).

## Server initialization parameters

`XrdSecProtocoloidcInit()` supports:

- `-maxsz <num>`: maximum token size (default 8192, max 524288)
- `-expiry {ignore|optional|required}`:
  - `ignore` = do not enforce the expiry claim (a present `exp` is not checked)
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
- `-clock-skew <seconds>`: allowed clock skew for time-based claims (default 60,
max 3600)
- `-identity-claim <claim>`: add claim names (in order) used for user identity;
may be specified multiple times
- `-forced-identity-claim <claim>`: for the current `-issuer`, force identity
extraction to this single claim (no fallback order for that issuer)
  - special case: when set to `email`, the email value is mapped to local
  username via `-email-map` entries or `[email-map]` in `oidc.cfg`
- `-base-path <path>`: for the current `-issuer`, a single absolute path;
exported as `base_path` on `XrdSecEntity` when defined (a later `-base-path`
replaces any previous value for that issuer)
- `-restricted-path <path>`: for the current `-issuer`, one absolute path; may be
repeated (like `-audience`); exported as a JSON array on `restricted_path` when
at least one path is defined
- `-email-map <email>=<username>`: map a token `email` claim (normalized to
lowercase) to a local username; may be repeated (also available as
`[email-map]` in the INI file)
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
`-forced-identity-claim`, `-base-path`, and `-restricted-path` are also
issuer-scoped. Repeat `-restricted-path` (or `restricted_path` lines in an
`[issuer ...]` INI section) to configure multiple restricted paths; each issuer
has at most one `base_path`.

Multiple `sec.protparm oidc` lines in `xrootd.cf` are supported (XRootD joins them
with newlines). List every `sec.protparm oidc` line **before** `sec.protocol oidc`,
for example:

```conf
sec.protparm oidc -issuer https://auth.cern.ch/auth/realms/cern
sec.protparm oidc -audience eos-service -expiry required -show-token-claims
sec.protparm oidc -forced-identity-claim email -email-map user@example.org=localuser
sec.protocol oidc
```

## TLS requirement

`oidc` rejects non-TLS connections. Both client and server constructors enforce
TLS-only use.

## HTTPS (XrdHttp) authentication

HTTPS bearer authentication is routed through the same `sec.protocol oidc` plugin
as `root://` (via the XRootD security framework). Configure OIDC once with
`sec.protparm oidc` / `sec.protocol oidc`, then enable HTTP bearer handling:

```conf
xrootd.seclib libXrdSec.so
xrootd.tls all
http.tlsclientauth off
http.header2cgi Authorization authz
http.oidc on

sec.protocol oidc
```

`http.oidc` modes:

- `on` or `optional` — validate `Authorization: Bearer <token>` when present
- `require` — reject requests without a valid bearer token (unless mTLS already
set the identity)

`sec.protocol oidc` (and any `sec.protparm oidc` lines) must be present in the
configuration; `http.oidc` only enables bearer-token handling over HTTPS.
Bearer tokens may be sent via the `Authorization` header (with `http.header2cgi`)
or as an `authz` CGI parameter.

On HTTP keep-alive connections, a changed `Authorization` bearer token is
detected and triggers re-validation, a new `XrdSecEntity`, and a fresh xrootd
`Bridge` login. Repeating the same token skips re-validation. Client-certificate
identities still take precedence and are not replaced by bearer tokens.

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

## CERN SSO vs WLCG IAM

CERN operates two related but distinct OIDC ecosystems:

| | CERN SSO (Keycloak) | WLCG IAM (INDIGO IAM) |
| - | --------------------- | ---------------------- |
| Role | Login / identity for CERN apps | VO-scoped tokens for WLCG storage/compute |
| Typical issuer | `https://auth.cern.ch/auth/realms/cern` | VO-specific IAM URL (e.g. `https://wlcg.cloud.cnaf.infn.it/`) |
| Token for XRootD | `id_token` or SSO `access_token` | WLCG-profile **`access_token` JWT** |
| Key claims | `sub`, `email`, `preferred_username` | `scope` (`storage.*`), `wlcg.ver`, `sub` |
| Storage authorization | **No** `storage.*` scopes in SSO tokens | **Yes**, with `XrdAccToken` |

`XrdSecoidc` accepts **both** issuers when configured. Use CERN SSO for
authentication-only setups. For EOS/XRootD path authorization, clients must
present an **IAM access token** with WLCG storage scopes — a CERN SSO
`id_token` from `xrdtoken CERNOIDC` is not sufficient on its own.

### WLCG IAM server configuration

**`xrootd.cf` (authentication + claim export):**

```conf
xrootd.seclib libXrdSec.so
xrootd.tls all

sec.protparm oidc -issuer https://wlcg.cloud.cnaf.infn.it/
sec.protparm oidc -audience <iam-client-id>
sec.protparm oidc -expiry required
sec.protparm oidc -entity-claim scope
sec.protparm oidc -entity-claim wlcg.ver
sec.protocol oidc

ofs.authorize
ofs.authlib libXrdAccToken.so
acctoken.basepath /eos/user
acctoken.onmissing deny
```

**INI equivalent:** see `src/XrdSecoidc/configs/wlcg-iam.example.cfg`.

**Client token acquisition:**

```sh
./utils/xrdtoken WLCG \
  --client-id <iam-client-id> \
  --scope "storage.read:/public storage.modify:/alice"
# or: xrdtoken IAM --issuer https://<vo-iam>/ --client-id ... --scope ...
```

The stored JWT should contain `wlcg.ver` and a space-separated `scope` claim
(or a JSON array of scope strings, which is normalized on export).

### Dual-issuer deployments

A single server may configure multiple `[issuer ...]` blocks — for example CERN
SSO for interactive identity checks and a WLCG IAM issuer for storage tokens.
Each issuer may define its own `base_path` and `restricted_path` values.

## Standard CERN SSO configuration

Use this for CERN Keycloak identity only (not WLCG storage scopes):

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

INI `oidc.cfg` equivalent using the CERN issuer and default mapping:

```ini
[issuer "https://auth.cern.ch/auth/realms/cern"]
audience = public-client
```

## Standard Google configuration

Example `oidc.cfg` using Google issuer + email mapping:

```ini
[issuer "https://accounts.google.com"]
audience = foo.apps.googleusercontent.com
forced-identity-claim = email

[email-map]
foo.bar@gmail.com = foo
```

Google app setup (Google Cloud Console):

1. Create/select a project.
2. Configure OAuth consent screen.
3. Create OAuth client credentials:
  - HTTPfor `xrdtoken ... GOOGLE` default flow (`--flow device`), use a client type
   that supports device authorization and provide both `client_id` and
   `client_secret` to `xrdtoken`.
  - for `--flow pkce`, use a Desktop app client id.
4. Copy client id/secret and use them in `xrdtoken` options (or env vars).

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
./utils/xrdtoken create ./build/google.token GOOGLE \
  --client-id "<google-oauth-client-id>" \
  --client-secret "<google-oauth-client-secret>"

# 2) Inspect token claims
./utils/xrdtoken show ./build/google.token

# 3) Try access with xrdfs
BEARER_TOKEN_FILE="$PWD/build/google.token" \
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

- `[issuer "..."]` blocks (issuer/audience/OIDC+JWKS URLs/forced-identity-claim/
base_path/restricted_path)
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
base_path = /tree1/
restricted_path = /public/
restricted_path = /shared/
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
- must not be writable by group or others,
- is opened with `O_NOFOLLOW`, so a symlink at the config path is rejected.

When `jwks-cache-file` is configured, cached keys are stored per issuer in that
file and reused across refresh/restart cycles until TTL expiry.

## Security considerations

- **Audience binding is only enforced when configured.** If an issuer has no
`-audience` (or `audience =`) entry, the `aud` claim is **not** checked and
any signed, unexpired token from that trusted issuer is accepted regardless of
its audience. This means a token minted for a different relying party at the
same issuer would be accepted by this server. Configure at least one
`-audience` per issuer to bind tokens to this service. When one or more
audiences are set, a token is accepted only if its `aud` (string or array
member) matches one of them, and a token lacking an `aud` claim is rejected.

## Notes

- Protocol id on the wire is `oidc`.
- Accepted JWT signature algorithm is currently `RS256`; tokens using any other
`alg` (including `none` or HMAC variants) or with no `alg` are rejected.
- OIDC discovery and JWKS endpoints must use `https://`.
- Server-side token cache is enabled by default and keyed by the SHA-256 hash
of the token (the raw token value is never used as a map key).
- Client-side debug logging can be enabled by setting the `XrdSecDEBUG`
environment variable to a truthy value (e.g. `1`, `on`, `true`, `yes`); it
logs which token source/file the client selected.

## Local helper scripts

For quick manual testing, helper scripts are available in `utils/`:

- `utils/xrdtoken create [tokenfile] [CERN|CERNOIDC|GOOGLE|GITHUB|IAM|WLCG]`:
creates token (`CERN` uses OAuth2 access token flow, `CERNOIDC` requests OIDC
scopes and requires/stores `id_token`, `GOOGLE` defaults to device flow, `GITHUB`
stores an opaque OAuth `access_token`, `IAM`/`WLCG` request WLCG storage
scopes and store the IAM `access_token` JWT).
- Shortcuts: `utils/xrdtoken CERNOIDC [tokenfile]`, `utils/xrdtoken WLCG ...`,
`utils/xrdtoken IAM ...` (IAM and WLCG are aliases).
- `utils/xrdtoken show [tokenfile]`: decodes and prints JWT header/payload.
- If `<tokenfile>` is omitted, default is `${XDG_RUNTIME_DIR}/bt_u<uid>`
(or `/tmp/bt_u<uid>` when `XDG_RUNTIME_DIR` is unset).
- For `GOOGLE`, provide `--client-id` (or set `GOOGLE_OAUTH_CLIENT_ID`).
- For `GOOGLE` default device flow, also provide `--client-secret` (or set
`GOOGLE_OAUTH_CLIENT_SECRET`).
- `GOOGLE` flow can be selected via `--flow device|pkce` (`device` default).
- `--client-secret` is optional for PKCE but may be required for some client
types/endpoints; env fallback: `GOOGLE_OAUTH_CLIENT_SECRET`.
- For `GOOGLE`, `xrdtoken` stores `id_token` when present (JWT), otherwise
falls back to `access_token`.
- For `GITHUB`, provide `--client-id` (or set `GITHUB_OAUTH_CLIENT_ID`). Device
flow does not need a client secret; enable **Device flow** in the OAuth app
settings. `GITHUB` supports `--flow device|pkce` (`device` default); PKCE
requires `--client-secret` (or `GITHUB_OAUTH_CLIENT_SECRET`).
- Device flow prints a prefilled verification URL when possible; GitHub prints
the verification page URL and a separate user code to enter manually.
- For `IAM`/`WLCG`, provide `--client-id` (or `WLCG_IAM_CLIENT_ID`) and a
required `--scope` with WLCG storage scopes. Override the IAM issuer with
`--issuer` (or `WLCG_IAM_ISSUER`; default CNAF test instance).

Example:

```sh
./utils/xrdtoken create
./utils/xrdtoken CERNOIDC
./utils/xrdtoken WLCG \
  --client-id "<iam-client-id>" \
  --scope "storage.read:/public storage.modify:/alice"
./utils/xrdtoken IAM \
  --issuer https://wlcg.cloud.cnaf.infn.it/ \
  --client-id "<iam-client-id>" \
  --scope "storage.read:/"
./utils/xrdtoken create GOOGLE \
  --client-id "<google-oauth-client-id>" \
  --client-secret "<google-oauth-client-secret>"
./utils/xrdtoken create GOOGLE --flow pkce \
  --client-id "<google-oauth-client-id>"
./utils/xrdtoken create GITHUB \
  --client-id "<github-oauth-client-id>"
./utils/xrdtoken show 
```


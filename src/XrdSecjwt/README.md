# XrdSecJWT

`XrdSecJWT` is an XRootD security protocol plugin (`sec.protocol jwt`) that
authenticates bearer-style JWT tokens over TLS.

This implementation validates JWTs directly in the plugin (no SciTokens
helper dependency).

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
- opened with `O_NOFOLLOW` (a symlink at the token path is rejected),
- checked with `fstat` on that descriptor (not a second path lookup),
- rejected before it is read when the size is empty or larger than `-maxsz`.

## Server initialization parameters

`XrdSecProtocoljwtInit()` supports:

- `-maxsz <num>`: maximum token size (default 8192, max 524288); enforced on
both the client and the server side
- `-expiry {ignore|optional|required}` (default `required`):
  - `ignore` = do not enforce the expiry claim (a present `exp` is not checked).
  An expired token is then accepted until the process restarts. Do not use this
  in production.
  - `optional` = enforce only if expiry present
  - `required` = expiry must be present and valid
- `-issuer <url>`: expected `iss` claim value, matched exactly. If
`-jwt-config-url` is not specified, the discovery URL is that issuer string
with `/.well-known/openid-configuration` appended (a trailing slash is not
doubled; the suffix is not moved to the host root). Discovery and JWKS fetches
follow at most three redirects, and only to `https://` targets. The server
certificate is verified. The `issuer` value in the discovery document must
match the configured issuer; a single trailing slash on either side is
ignored. If the issuer is not signed by a public CA, set
`SSL_CERT_FILE` (or `CURL_CA_BUNDLE`) to a PEM bundle before starting
`xrootd`; the plugin passes that path to libcurl.
- `-audience <value>`: expected token audience (`aud` string or array member);
may be repeated for the current issuer. **If no audience is set for an
issuer, `aud` is not checked** and any otherwise valid token from that issuer
is accepted, including one minted for a different client. Set at least one
audience on every production issuer.
- `-jwt-config-url <https-url>`: OpenID discovery URL (used to locate JWKS URI)
- `-jwks-url <https-url>`: explicit JWKS endpoint (overrides discovery lookup)
- `-jwks-refresh <seconds>`: JWKS refresh cache interval (default 300)
- `-jwks-refresh-min <seconds>`: minimum interval between *forced* JWKS
refreshes per issuer, and between retries after a failed periodic refresh
(default 30; `0` disables the cooldown). A forced refresh is triggered when
a token's signature does not verify with the cached keys (suspected key
rotation). The cooldown stops unauthenticated clients from turning garbage
tokens into a fetch storm against the issuer; while it is in effect such
tokens fail immediately with a signature error. After a periodic refresh
fails, the same interval keeps the keys already in memory in use instead of
contacting the issuer again. The cooldown is waived while no keys are held
at all.
- `-jwks-cache-file <path>`: optional on-disk JWKS cache file shared across
issuers (disabled by default)
- `-jwks-cache-ttl <seconds>`: TTL for on-disk cached issuer keys
(`0` = use `-jwks-refresh`; default 0)
- `-clock-skew <seconds>`: allowed clock skew for time-based claims (default 60,
max 3600)
- `-identity-claim <claim>`: add claim names (in order) used for user identity;
may be specified multiple times
- `-entity-claim <claim>` or `-entity-claim <claim>=<attribute>`: copy a JWT
claim onto `XrdSecEntity` for a later authorization plugin. Without
`=<attribute>` the key is `token.<claim>` (`scope` becomes `token.scope`,
`wlcg.ver` becomes `token.wlcg.ver`). A string is copied as-is; a JSON array
of strings is joined with single spaces. May be repeated. This option is
global, not per issuer, and is applied only at startup (including
`entity-claim` lines in `[global]`). It is not part of the reloadable INI
sections.
- `-forced-identity-claim <claim>`: for the current `-issuer`, force identity
extraction to this single claim (no fallback order for that issuer)
  - special case: when set to `email`, the email value is mapped to local
  username via `-email-map` entries or `[email-map]` in `jwt.cfg`
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
- `-config-file <path>`: load the `jwt.cfg` INI from this path (instead of
`/etc/xrootd/jwt.cfg`). Reloads `[issuer ...]` and `[email-map]` when the file
changes. Do not combine with `-scitokens-config`.
- `-scitokens-config <path>`: off unless set. Read `path` as a SciTokens INI
and export what `XrdAccjwt` already uses. This does not load `jwt.cfg` and
does not call the SciTokens library. See below.

Configuration can be supplied in four ways. A SciTokens file is not mixed
with `jwt.cfg`. The other ways are not mixed unless `-config-file` is set:

- arguments on `sec.protocol jwt` itself,
- one or more `sec.protparm jwt` lines, which XRootD joins with newlines and
passes as the protocol parameters (list them **before** `sec.protocol jwt`),
- no parameters at all, in which case `/etc/xrootd/jwt.cfg` is required,
- `-scitokens-config <path>`, which replaces `jwt.cfg` (see below). Extra
options such as `-expiry` may still follow it.

Inline parameters do not also load `/etc/xrootd/jwt.cfg`. `-config-file <path>`
loads that INI file and appends any other parameters after it. INI keys map to
the options above. `-scitokens-config` is a different file format; it is not
combined with `-config-file` or with the default `jwt.cfg`.

## SciTokens configuration file

`-scitokens-config /etc/xrootd/scitokens.cfg` turns on a compatibility reader
for the INI used by `libXrdAccSciTokens.so`. The switch is not the default.
The same file can stay in place for sites that already have one. `XrdSecjwt`
only uses it to authenticate and to fill `XrdSecEntity`. `libXrdAccjwt.so`
still makes the access decision. `scitokens-cpp` is not used.

```conf
sec.protocol jwt -scitokens-config /etc/xrootd/scitokens.cfg
ofs.authorize
ofs.authlib libXrdAccjwt.so
accjwt.onmissing deny
```

| SciTokens key | Effect |
|---|---|
| `[Issuer <name>]` and `issuer = <url>` | One issuer. The name is a label. A later section with the same issuer URL replaces the earlier one. `issuer` and `base_path` are required. |
| `[Global] audience`, `audience_json` | Audience list copied onto every issuer. With no audience, `aud` is not checked. |
| `base_path` | Entity attribute `base_path`. One absolute path is a string. Several, separated by commas or spaces, are a JSON array. |
| `restricted_path` | Entity attribute `restricted_path` (JSON array). |
| `username_claim` | Identity is taken from that claim. |
| `map_subject = true` | Identity is taken from `sub`, unless `username_claim` is set. |

Every accepted token also exports `scope` as `token.scope` and, when present,
`wlcg.ver` as `token.wlcg.ver`. `name_mapfile`, `default_user`, `groups_claim`,
`authorization_strategy`, and `onmissing` are ignored. Missing-token behavior
stays `accjwt.onmissing`.

The file is watched the same way as `jwt.cfg`. On each authentication the
plugin stats it, and a change of inode, mtime, ctime, or size re-reads the
issuers: issuer URL, audience, `base_path`, `restricted_path`,
`username_claim`, and `map_subject`. The new policies replace the old ones
and the validated-token cache is dropped. Inline `-issuer` options and
`-email-map` entries from startup are applied again on top of the file, so an
issuer or audience that exists only on the command line is not dropped.
`scope` and `wlcg.ver` export and `-expiry` stay as they were at startup. A reload
that fails to parse, or whose issuer keys cannot be fetched, keeps the
previous file and is not tried again until the file changes. It does not
drop an existing xroot session. On HTTP, a
connection that keeps sending the same token text keeps the entity already
installed on that connection.

In the `jwt.cfg` dialect, `-issuer` starts a new issuer policy block. `-audience`, `-jwt-config-url`,
`-jwks-url`, `-forced-identity-claim`, `-base-path`, and `-restricted-path`
apply to the current issuer and are rejected if they appear before the first
`-issuer` (`requires a prior -issuer`). Repeat `-restricted-path` (or
`restricted_path` lines in an `[issuer ...]` INI section) to configure
multiple restricted paths; each issuer has at most one `base_path`. Global
options (`-expiry`, `-entity-claim`, cache and JWKS timings) may appear
anywhere.

For example:

```conf
sec.protparm jwt -issuer https://auth.cern.ch/auth/realms/cern
sec.protparm jwt -audience eos-service -expiry required -show-token-claims
sec.protparm jwt -forced-identity-claim email -email-map user@example.org=localuser
sec.protocol jwt
```

## TLS requirement

`jwt` rejects non-TLS connections. The xroot protocol reports `needTLS`, so a
`root://` login without TLS is refused. `http.jwt` runs only on an HTTPS
connection, after the TLS handshake.

## HTTPS (XrdHttp) authentication

HTTPS bearer authentication is routed through the same `sec.protocol jwt` plugin
as `root://` (via the XRootD security framework). Configure JWT once with
`sec.protparm jwt` / `sec.protocol jwt`, then enable HTTP bearer handling:

```conf
xrootd.seclib libXrdSec.so
xrootd.tls all
http.tlsclientauth off
http.jwt on

sec.protparm jwt -issuer https://issuer.example
sec.protparm jwt -audience xrootd -expiry required
sec.protocol jwt
```

`http.jwt` modes:

- `on` or `optional` — validate `Authorization: Bearer <token>` when present
- `require` — reject requests without a valid bearer token (unless mTLS already
set the identity)

`sec.protocol jwt` (and any `sec.protparm jwt` lines) must be present in the
configuration; `http.jwt` only enables bearer-token handling over HTTPS.
`http.jwt` reads the `Authorization` header directly. `http.header2cgi Authorization authz`
is only needed when an authorization plugin must see the token as authz CGI.
Bearer tokens may be sent via the `Authorization` header
or as an `authz` CGI parameter on the request URL (`?authz=`).

Bearer identity is checked once per request, after the headers have been
read, including on HTTP keep-alive. It is not checked again while that
request's body is transferred. A repeat of the same token is a cache lookup,
but the request is still submitted to the plugin, so an expired token stops
working on the next request of a long-lived connection.

- A different token replaces the previous identity. Attributes of the old
token are discarded and a new xrootd Bridge login is performed.
- The same token, still valid, keeps the Bridge login and the entity already
installed on that connection. A later INI reload does not refresh that
entity while the token text is unchanged.
- A token that no longer validates (expired, bad signature, issuer removed,
audience no longer accepted) drops the previous bearer identity.
- In `on`/`optional` mode, a request with no token on a connection that had a
bearer identity returns to anonymous. In `require` mode that request is
rejected with 401.
- If `SecEntity.name` is already set and `SecEntity.prot` is not `jwt` (a
client certificate), the bearer token is not applied.

If the Bridge cannot be disconnected while the identity is replaced, the
request fails with 503 and the previous identity stays.

When `xrootd.seclib` is configured, XrdHttp reuses the xrootd protocol's
security framework instance (published in the shared xrd environment as
`XrdSecService*`), so `sec.protocol jwt` is initialized exactly once per
process. Only if none is available does XrdHttp load its own instance.

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
sec.protocol jwt \
  -issuer https://issuer-a.example \
  -audience xrootd \
  -audience xrootd-admin \
  -issuer https://issuer-b.example \
  -audience service-b \
  -expiry required
```

## CERN SSO vs WLCG IAM

CERN operates two related but distinct OpenID Connect ecosystems:

| | CERN SSO (Keycloak) | WLCG IAM (INDIGO IAM) |
| - | --------------------- | ---------------------- |
| Role | Login / identity for CERN apps | VO-scoped tokens for WLCG storage/compute |
| Typical issuer | `https://auth.cern.ch/auth/realms/cern` | VO-specific IAM URL (e.g. `https://wlcg.cloud.cnaf.infn.it/`) |
| Token for XRootD | `id_token` or SSO `access_token` | WLCG-profile **`access_token` JWT** |
| Key claims | `sub`, `email`, `preferred_username` | `scope` (`storage.*`), `wlcg.ver`, `sub` |
| Storage authorization | **No** `storage.*` scopes in SSO tokens | **Yes**, with `XrdAccjwt` |

`XrdSecjwt` accepts **both** issuers when configured. An SSO `id_token` (or SSO
access token) only proves identity: CERN SSO tokens do not carry `storage.*`
scopes. Path authorization needs a WLCG IAM **access token** whose `scope` and
`wlcg.ver` claims are exported to `XrdSecEntity`. A CERN SSO `id_token` from
`xrdjwt CERNOIDC` is not a substitute for that access token.

### WLCG IAM server configuration

**`xrootd.cf` (authentication + claim export):**

```conf
xrootd.seclib libXrdSec.so
xrootd.tls all

sec.protparm jwt -issuer https://wlcg.cloud.cnaf.infn.it/
sec.protparm jwt -audience <iam-client-id>
sec.protparm jwt -expiry required
sec.protparm jwt -entity-claim scope
sec.protparm jwt -entity-claim wlcg.ver
sec.protparm jwt -base-path /eos
sec.protocol jwt

ofs.authorize
ofs.authlib libXrdAccjwt.so
accjwt.basepath /eos
accjwt.onmissing deny
```

`-entity-claim scope` stores the token's `scope` claim on the entity as
`token.scope` (a JSON array becomes a space-separated list, for example
`storage.read:/public storage.modify:/alice`). `-entity-claim wlcg.ver` stores
`token.wlcg.ver`. `-base-path` stores the issuer policy path as `base_path`.
This plugin does not interpret those values. `libXrdAccjwt.so` does, and
`accjwt.basepath` is that library's own prefix, not a `sec.protocol jwt`
option and not the same setting as `-base-path`. Point it at the tree under
which the exported scopes are meant to apply.

**INI equivalent:** see `src/XrdSecjwt/configs/wlcg-iam.example.cfg`.

**Client token acquisition** (IAM access token, not an SSO id token):

```sh
./utils/xrdjwt WLCG \
  --client-id <iam-client-id> \
  --scope "storage.read:/public storage.modify:/alice"
# or: xrdjwt IAM --issuer https://<vo-iam>/ --client-id ... --scope ...
```

The stored JWT should contain `wlcg.ver` and a space-separated `scope` claim
(or a JSON array of scope strings, which is normalized on export).

### Dual-issuer deployments

A single server may configure multiple `[issuer ...]` blocks — for example CERN
SSO for interactive identity checks and a WLCG IAM issuer for storage tokens.
Each issuer may define its own `base_path` and `restricted_path` values.

## Standard CERN SSO configuration

Use this for CERN Keycloak identity only (not WLCG storage scopes). One
complete file is enough; do not split `[global]` and `[issuer]` across
fragments that omit the issuer.

Inline `xrootd.cf` style:

```conf
sec.protocol jwt \
  -issuer https://auth.cern.ch/auth/realms/cern \
  -audience public-client \
  -expiry required \
  -show-token-claims
```

INI equivalent (`/etc/xrootd/jwt.cfg`, used when `sec.protocol jwt` has no
parameters):

```ini
[global]
expiry = required
show-token-claims = true

[issuer "https://auth.cern.ch/auth/realms/cern"]
audience = public-client
```

## Standard Google configuration

Example `jwt.cfg` using Google issuer + email mapping. `expiry` defaults to
`required` when omitted; it is set here so the file stands alone:

```ini
[global]
expiry = required

[issuer "https://accounts.google.com"]
audience = foo.apps.googleusercontent.com
forced-identity-claim = email

[email-map]
foo.bar@gmail.com = foo
```

Google app setup (Google Cloud Console):

1. Create or select a project and configure the OAuth consent screen.
2. Preferred: `xrdjwt GOOGLE --flow pkce` with an OAuth client of type
**Desktop**. Pass its client id. A client secret is optional for PKCE.
3. Default `--flow device` works only when Google has enabled the device-code
grant for that client (historically a "TVs and Limited Input devices"
client). A web or desktop client id is rejected by Google's device endpoint.
When the grant is enabled, pass both the client id and the client secret.
4. `xrdjwt` stores `id_token` when the response includes one. That JWT is
what this plugin validates, and its `aud` is the OAuth client id. If Google
returns only an opaque `access_token`, the helper stores that instead and
`sec.protocol jwt` rejects it. Run `xrdjwt show` on the file before using
it.

Quickstart (Google, PKCE, identity only):

```sh
# 0) Server config. sec.protocol jwt with no parameters requires jwt.cfg,
#    and that file must name the issuer and audience or every token is rejected.
cat > /etc/xrootd/jwt.cfg <<'EOF'
[global]
expiry = required

[issuer "https://accounts.google.com"]
audience = foo.apps.googleusercontent.com
forced-identity-claim = email

[email-map]
foo.bar@gmail.com = foo
EOF

cat > /etc/xrootd/xrootd.cf <<'EOF'
xrootd.seclib libXrdSec.so
all.role server
sec.protocol jwt
xrootd.tls all
xrd.tlsca certdir /etc/grid-security/certificates
xrd.tls /etc/grid-security/xrd/xrdcert.pem /etc/grid-security/xrd/xrdkey.pem
EOF

# 0b) Start the server (foreground example)
xrootd -c /etc/xrootd/xrootd.cf -R xrootd

# 1) Create a token (Desktop client, PKCE). audience above is this client id.
./utils/xrdjwt create ./build/google.token GOOGLE --flow pkce \
  --client-id "<google-oauth-client-id>"

# 2) Confirm the file is a JWT (three base64url segments), not an opaque token
./utils/xrdjwt show ./build/google.token

# 3) Try access with xrdfs
BEARER_TOKEN_FILE="$PWD/build/google.token" \
LD_LIBRARY_PATH=/usr/local/lib64 \
/usr/local/bin/xrdfs localhost:2000 stat /tmp/
```

## INI fallback file (`/etc/xrootd/jwt.cfg`)

When `sec.protocol jwt` has no trailing parameters, this file is required and
loaded at plugin init time. If the file is missing, initialization fails.

You can override this path with:

```conf
sec.protocol jwt -config-file /path/to/jwt.cfg
```

When `jwt.cfg` is used (default path or `-config-file`), the plugin checks
inode, mtime, ctime, and size at authentication time and reloads only:

- `[issuer "..."]` blocks, and `issuer = <url>` at the top level or in
`[global]` (audience, discovery URL, JWKS URL, forced-identity-claim,
base_path, restricted_path)
- `[email-map]`

`[global]` keys, including `expiry` and `entity-claim`, are intentionally **not**
reloaded and remain fixed from startup. A reload that parses and validates
replaces the issuer policies and the email map, then applies inline `-issuer`
options and inline `-email-map` entries from startup on top of that file.
It drops the validated-token cache, so the next authentication recomputes
identity, audience, and
`base_path` / `restricted_path`. It does not disconnect an existing xroot
session. On HTTP, the next request revalidates as described above: failure
drops the bearer identity, while the same still-valid token keeps the entity
already installed on that connection.

```ini
[global]
maxsz = 8192
expiry = required
jwks-refresh = 300
jwks-refresh-min = 30
jwks-cache-file = /var/lib/xrootd/jwt-jwks-cache.ini
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
# jwt-config-url = https://issuer-a.example/.well-known/openid-configuration
# jwks-url = https://issuer-a.example/protocol/openid-connect/certs

[issuer "https://issuer-b.example"]
audience = service-b

[email-map]
alice@example.org = alice
bob@example.org = bobby
```

Supported sections are `[global]` and `[issuer "<issuer-url>"]` (or `[issuer]`
with `issuer = <url>` inside the section), plus `[email-map]` for
`forced-identity-claim = email`. An `issuer = <url>` line at the top level or
in `[global]` starts an issuer as well, and a reload keeps it. Boolean keys
accept `true/false`, `yes/no`, `on/off`, `1/0`.

Security checks for `/etc/xrootd/jwt.cfg` (applied with `fstat` after
`O_NOFOLLOW` open):

- must be a regular file,
- must be owned by the effective uid of the process at the moment it is
opened. After xrootd drops privileges that is the daemon user, not root: a
root-owned file is rejected,
- must not be writable by group or others (other-read is allowed),
- a symlink at the config path is rejected.

When `jwks-cache-file` is configured, cached keys are stored per issuer in that
file and reused across refresh/restart cycles until TTL expiry.

## Security considerations

- **Audience binding is only enforced when configured.** If an issuer has no
`-audience` (or `audience =`) entry, the `aud` value is **not** compared and
any signed, unexpired token from that trusted issuer is accepted regardless of
which resource it names. Configure at least one `-audience` per issuer. When
one or more audiences are set, a token is accepted only if its `aud` (string
or array member) matches one of them, and a token lacking an `aud` claim is
rejected. A WLCG token (`wlcg.ver` present) is rejected when `aud` is missing
even if no audience is configured; a WLCG token that does carry `aud` is
still accepted in that case, so an existing WLCG issuer does not start
failing only because `-audience` was left unset.
- **`crit` is rejected.** The plugin implements no JOSE header extensions. A
token with a `crit` member is refused. WLCG and SciTokens access tokens do
not set `crit`.
- **`azp` is required only for a multi-audience ID token.** When `aud` is an
array of more than one value and the token carries the `openid` scope (or
`nonce` / `at_hash`), `azp` must be present and must be one of those
audiences. A WLCG or SciTokens storage token (`wlcg.ver`, `storage.*`,
`read:`, or `write:`) is not subject to that check, and neither is an access
token that is not an OpenID ID token.
- **`-expiry ignore` disables expiry.** That switch remains available and is
off by default. Prefer `-expiry required`, which is what WLCG tokens need.

## Notes

- Protocol id on the wire is `jwt`.
- Accepted JWT signature algorithms are `RS256/RS384/RS512` (RSASSA-PKCS1-v1_5),
`PS256/PS384/PS512` (RSASSA-PSS) and `ES256/ES384/ES512` (ECDSA on P-256/P-384/
P-521). The key type and, for ECDSA, the curve must match the algorithm. Tokens
using any other `alg` (including `none` and the HMAC family) or with no `alg`
are rejected.
- JWKS entries of type `RSA` and `EC` are loaded. A token whose `kid` matches a
key is checked against that key only. A token whose `kid` is unknown is checked
only against entries that have no `kid`. A token with no `kid` is checked
against every loaded key. The key type and, for ECDSA, the curve must match
`alg`.
- Cached keys younger than `-jwks-refresh` are used with no network access.
Once they are older, one refresh runs. If that fetch fails, the keys already
in memory are left in place and this validation fails, unless a
`jwks-cache-file` entry that is still inside its TTL can be loaded. The
failure is remembered: for `-jwks-refresh-min` seconds further periodic
refreshes are skipped and validation uses the keys already in memory.
Concurrent callers share that one failed fetch instead of each contacting
the issuer. A signature mismatch triggers one forced refresh, limited by
the same interval; during that cooldown the token fails immediately and no
fetch is started. The cooldown is waived while no keys are held at all.
Fetches do not run while the per-issuer key lock is held.
- The `Bearer` scheme prefix is matched case-insensitively (RFC 6750).
- `exp`/`nbf` may be integers or floating-point NumericDate values.
- OpenID discovery and JWKS endpoints must use `https://`.
- Server-side token cache is enabled by default and keyed by the SHA-256 hash
of the token (the raw token value is never used as a map key; if the digest
fails, validation fails). Entries live until `exp + clock-skew`, or for
`-token-cache-noexp-ttl` when `exp` is absent or ignored. With `-expiry
ignore`, an expired token is cached for that no-exp TTL. The cache stores the
identity computed at validation time. Entity attributes (`token.*`,
`base_path`, `restricted_path`) are rebuilt from the cached payload and the
issuer policy on each hit. An INI reload clears the cache.
- Config-file reload detection uses inode, mtime, ctime and size; only one
thread performs a reload at a time.
- Client-side debug logging can be enabled by setting the `XrdSecDEBUG`
environment variable to a truthy value (e.g. `1`, `on`, `true`, `yes`); it
logs which token source/file the client selected.

## Local helper scripts

For quick manual testing, helper scripts are available in `utils/`:

- `utils/xrdjwt create [tokenfile] [CERN|CERNOIDC|GOOGLE|GITHUB|IAM|WLCG]`:
creates token (`CERN` uses OAuth2 access token flow, `CERNOIDC` requests OpenID
scopes and requires/stores `id_token`, `GOOGLE` defaults to device flow, `GITHUB`
stores an opaque OAuth `access_token`, `IAM`/`WLCG` request WLCG storage
scopes and store the IAM `access_token` JWT).
- Shortcuts: `utils/xrdjwt CERNOIDC [tokenfile]`, `utils/xrdjwt WLCG ...`,
`utils/xrdjwt IAM ...` (IAM and WLCG are aliases).
- `utils/xrdjwt show [tokenfile]`: decodes and prints JWT header/payload.
- If `<tokenfile>` is omitted, default is `${XDG_RUNTIME_DIR}/bt_u<uid>`
(or `/tmp/bt_u<uid>` when `XDG_RUNTIME_DIR` is unset).
- For `GOOGLE`, provide `--client-id` (or set `GOOGLE_OAUTH_CLIENT_ID`).
- For `GOOGLE` default device flow, also provide `--client-secret` (or set
`GOOGLE_OAUTH_CLIENT_SECRET`).
- `GOOGLE` flow can be selected via `--flow device|pkce` (`device` default).
Google's device-code endpoint rejects client types that do not have that
grant; `--flow pkce` with a Desktop client is the reliable choice. The stored
value is `id_token` when present, otherwise `access_token`, which may be
opaque and is then rejected by this plugin.
- `--client-secret` is optional for PKCE but may be required for some client
types/endpoints; env fallback: `GOOGLE_OAUTH_CLIENT_SECRET`.
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
./utils/xrdjwt create
./utils/xrdjwt CERNOIDC
./utils/xrdjwt WLCG \
  --client-id "<iam-client-id>" \
  --scope "storage.read:/public storage.modify:/alice"
./utils/xrdjwt IAM \
  --issuer https://wlcg.cloud.cnaf.infn.it/ \
  --client-id "<iam-client-id>" \
  --scope "storage.read:/"
./utils/xrdjwt create GOOGLE \
  --client-id "<google-oauth-client-id>" \
  --client-secret "<google-oauth-client-secret>"
./utils/xrdjwt create GOOGLE --flow pkce \
  --client-id "<google-oauth-client-id>"
./utils/xrdjwt create GITHUB \
  --client-id "<github-oauth-client-id>"
./utils/xrdjwt show 
```


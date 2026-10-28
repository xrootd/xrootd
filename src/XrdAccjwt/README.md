# XrdAccjwt

`XrdAccjwt` is an XRootD **authorization** plugin (`ofs.authlib`) that enforces
JWT scope claims on storage paths. It works together with `XrdSecjwt`, which
handles **authentication** and JWT validation.

After `XrdSecjwt` has authenticated a client, selected JWT claims are available
as extended attributes on the `XrdSecEntity`. `XrdAccjwt` reads those attributes
and decides whether the requested operation on a path is permitted. An xroot
open that carries `authz=` is an exception: that value is checked with
`XrdSecJWT::ValidateToken`, and the resulting attributes are used for that
operation. The login attributes are not used for the decision, and a rejected
token follows `accjwt.onmissing`.

```
Client ──► XrdSecjwt (authenticate, validate JWT)
              │
              ▼
         XrdSecEntity
         • name        (identity)
         • token.scope (authorization scopes)
         • token.wlcg.ver (profile marker, if present)
         • base_path   (optional issuer namespace, from jwt)
         • restricted_path (optional JSON path allow-list, from jwt)
              │
              ▼
         XrdAccjwt (authorize path + operation)
```

This separation keeps token validation in one place and allows authorization
logic to be swapped independently via `ofs.authlib`.

---

## Features

- **WLCG Token Profile** — `storage.read:/path`, `storage.create:/path`,
  `storage.modify:/path`, `storage.stage:/path`, `storage.poll:/path`
- **SciTokens** — `read:/path`, `write:/path`
- **Automatic profile detection** — WLCG vs SciTokens is chosen from the
  `wlcg.ver` claim
- **Subtree path matching** — scopes authorize an entire directory subtree
- **Site base path** — optional `accjwt.basepath` prefix applied to all scope
  paths; when the entity also carries `base_path`, the configured base path is
  applied first and the entity value is appended
  (e.g. site `/atlas` + entity `/eos` + scope `/data` → `/atlas/eos/data`).
  A JSON array of absolute paths is a set of alternative prefixes; the request
  is authorized when any one of them covers it
- **Restricted paths** — when the authentication plugin exports `restricted_path`
  as a JSON array, a request outside every listed path is not granted by a
  scope. It follows `onmissing`: `accjwt.onmissing allow`, or that issuer's
  `jwt.onmissing allow`, still grants it. Use `deny` when a path outside the
  list must be rejected.
- **Transfer monitoring** — a granted read, create, or update reports
  `XrdSecMonitor::TokenInfo` (`s`, `n`, `o`, `r`, `g`) from `token.subject`,
  `request.name` (or `Entity.name`), `vorg`, `role`, and `grps`
- **Plugin chaining** — can wrap another `ofs.authlib` (e.g. `libXrdAcc.so`)
- **HTTP support** — works with `root://` and HTTPS when jwt is configured

Non-storage scopes (`compute.*`, `queue`, `execute`, …) are ignored.

---

## Supported token profiles

### WLCG

WLCG tokens are identified by a non-empty `wlcg.ver` claim (stored on the
entity as `token.wlcg.ver` by default). When this claim is present, scopes use
the WLCG format:

```json
{
  "iss": "https://iam.example.org",
  "sub": "alice",
  "aud": "https://storage.example.org",
  "wlcg.ver": "1.0",
  "scope": "storage.read:/datasets storage.modify:/scratch storage.stage:/archive"
}
```

This grants:

- read access under `/datasets`
- full write access under `/scratch`
- tape staging operations under `/archive` (no read implied)

### SciTokens

When `wlcg.ver` is absent or empty, scopes are interpreted as SciTokens:

```json
{
  "iss": "https://issuer.example.org",
  "sub": "248289761001",
  "aud": "storage.example.org",
  "scope": "read:/datasets write:/scratch"
}
```

| Profile | Detected when | Scope format |
| ------- | ------------- | ------------ |
| WLCG | `wlcg.ver` is non-empty | `storage.<capability>:<path>` |
| SciTokens | `wlcg.ver` missing or empty | `read:<path>`, `write:<path>` |

---

## XrdSecjwt configuration

`XrdAccjwt` requires `XrdSecjwt` (`sec.protocol jwt`) to authenticate
clients and export JWT claims onto the `XrdSecEntity`.

### Required: export claims with `-entity-claim`

Use `-entity-claim` to copy JWT claims into entity attributes after successful
authentication. Without this, `XrdAccjwt` has no scope information to
evaluate.

| JWT claim | Default entity attribute | Required for |
| --------- | ------------------------ | ------------ |
| `scope` | `token.scope` | all tokens |
| `wlcg.ver` | `token.wlcg.ver` | WLCG profile detection |

The default mapping is `token.<claim>`. To use a custom attribute key, specify
`claim=attrkey`:

```conf
-entity-claim scope=token.scope
-entity-claim wlcg.ver=token.wlcg.ver
```

### Minimal `xrootd.cf` example

```conf
xrootd.seclib libXrdSec.so
xrootd.tls all

# JWT authentication — export claims needed by XrdAccjwt
sec.protparm jwt -issuer https://iam.example.org
sec.protparm jwt -audience https://storage.example.org
sec.protparm jwt -expiry required
sec.protparm jwt -entity-claim scope
sec.protparm jwt -entity-claim wlcg.ver
sec.protocol jwt

# Authorization
ofs.authorize
ofs.authlib libXrdAccjwt.so
```

For SciTokens-only deployments, `wlcg.ver` export is optional — only
`-entity-claim scope` is required.

The `scope` claim may be a space-separated string or a JSON array of strings;
`XrdSecjwt` normalizes arrays to a space-separated `token.scope` value.

### CERN SSO vs WLCG IAM

| Use case | Issuer | Client token |
| -------- | ------ | ------------ |
| CERN identity only | `auth.cern.ch` (Keycloak) | `xrdjwt CERNOIDC` (`id_token`) |
| WLCG storage paths | VO IAM issuer | `xrdjwt WLCG --scope "storage.read:/..."` |

See [`src/XrdSecjwt/README.md`](../XrdSecjwt/README.md) and
[`src/XrdSecjwt/configs/wlcg-iam.example.cfg`](../XrdSecjwt/configs/wlcg-iam.example.cfg)
for a full IAM example with `XrdAccjwt`.

### Optional: issuer `base_path` and `restricted_path`

When configured per issuer in `sec.protocol jwt` or `jwt.cfg`, jwt exports
these attributes automatically (no `-entity-claim` needed):

| Entity attribute | Source | Used by `XrdAccjwt` as |
| ---------------- | ------ | ------------------------ |
| `base_path` | issuer `-base-path` / `base_path =` | one prefix, or a JSON array of alternative prefixes, after `accjwt.basepath` |
| `restricted_path` | issuer `-restricted-path` / repeated `restricted_path =` | JSON array allow-list for the request path |

Example issuer block:

```ini
[issuer "https://iam.example.org"]
audience = https://storage.example.org
base_path = /eos
restricted_path = /atlas/eos/data/public/
restricted_path = /atlas/eos/data/shared/
```

With `accjwt.basepath /atlas` and scope `storage.read:/data`, the effective
scope path is `/atlas/eos/data`. `restricted_path` is compared with the full
request path. The site prefix and the issuer `base_path` are not prepended to
it. A request for `/atlas/eos/data/public/file` is allowed because it is under
both the scope and `/atlas/eos/data/public`.

Several SciTokens `base_path` values are exported as a JSON array, for example
`["/eos","/cms"]`. With the same site prefix the request is authorized under
`/atlas/eos/data` or `/atlas/cms/data`. A single absolute path is unchanged.

### Equivalent `jwt.cfg` (INI)

When using a config file instead of inline parameters:

```ini
[global]
expiry = required
entity-claim = scope
entity-claim = wlcg.ver

[issuer "https://iam.example.org"]
audience = https://storage.example.org
```

With inline `sec.protparm jwt` lines, list all parameters **before**
`sec.protocol jwt`. See the [XrdSecjwt README](../XrdSecjwt/README.md)
for full jwt options (issuers, audiences, JWKS, identity mapping, HTTP setup).
`http.jwt` reads the `Authorization` header directly. `http.header2cgi Authorization authz`
is only needed when an authorization plugin must see the token as authz CGI.

### HTTPS clients

For HTTPS access with the same jwt configuration:

```conf
http.tlsclientauth off
http.jwt on

sec.protparm jwt -issuer https://iam.example.org
sec.protparm jwt -audience https://storage.example.org
sec.protparm jwt -entity-claim scope
sec.protparm jwt -entity-claim wlcg.ver
sec.protocol jwt
```

---

## XrdAccjwt configuration

### `xrootd.cf` directives

```conf
accjwt.trace info
accjwt.basepath /atlas
accjwt.scopeattr token.scope
accjwt.wlcgverattr token.wlcg.ver
accjwt.onmissing passthrough
accjwt.strategy capability group mapping
```

| Directive | Default | Description |
| --------- | ------- | ----------- |
| `accjwt.trace` | `error,warning` | Logging: `all`, `error`, `warning`, `info`, `debug`, `none` |
| `accjwt.basepath` | *(empty)* | Site prefix prepended to every scope path |
| `accjwt.scopeattr` | `token.scope` | Entity attribute containing the space-separated `scope` claim |
| `accjwt.wlcgverattr` | `token.wlcg.ver` | Entity attribute for the `wlcg.ver` claim |
| `accjwt.onmissing` | `passthrough` | Behavior when this plugin does not authorize the request (see below) |
| `accjwt.strategy` | `capability group mapping` | Which SciTokens strategies apply. Overrides `jwt.strategy`. See below. |

### `ofs.authlib` parameters

Parameters on the `ofs.authlib` line override defaults:

```conf
ofs.authlib libXrdAccjwt.so base_path=/atlas,scope_attr=token.scope,wlcg_ver_attr=token.wlcg.ver,onmissing=deny
```

| Parameter | Description |
| --------- | ----------- |
| `base_path` | Same as `accjwt.basepath` |
| `scope_attr` | Same as `accjwt.scopeattr` |
| `wlcg_ver_attr` | Same as `accjwt.wlcgverattr` |
| `onmissing` | Same as `accjwt.onmissing` |
| `strategy` | Same as `accjwt.strategy`. Use `+` between words (`strategy=group+mapping`), because a comma or a space separates parameters. |

### `onmissing` behavior

`accjwt.onmissing` is the default when this plugin does not authorize the
request: the scope attribute is absent, the claim has no storage scopes, the
path is outside `restricted_path`, or a present scope does not cover the
operation. An issuer may set its own value. `sec.protocol jwt` exports that
as `jwt.onmissing`, and Accjwt uses it for tokens from that issuer. Issuers
that omit it keep `accjwt.onmissing`. A rejected `authz=` token has no issuer
policy, so it follows the global value. Groups copied onto the entity by the
authentication plugin are left in place, so a chained plugin can use them.

A malformed `base_path`, a malformed `restricted_path`, or a request path that
is not absolute is denied here and is not passed on.

A JOSE `typ` of `eos+jwt` (or `application/eos+jwt`) is not evaluated. Accjwt
does not check that token's signature, issuer, or scopes, and it does not
apply `accjwt.onmissing` or `jwt.onmissing`. The handshake `request.name` is
restored and the request is passed to the chained plugin, which has to accept
or reject the token. With no chained plugin the operation is denied. On an
xroot open the type is taken from the unverified `authz` header, so any
`authz` value marked `eos+jwt` skips this plugin. A login token this plugin
already accepted is recognized from `jwt.typ`.

| Value | Behavior |
| ----- | -------- |
| `passthrough` | Delegate to the chained authorization plugin (default) |
| `allow` | Grant the requested operation |
| `deny` | Deny the request |

### `strategy`

`accjwt.strategy` takes one or more of `capability`, `group`, and `mapping`.
The default is all three, which is also SciTokens' default.

`capability` is the only strategy that grants. A storage scope that covers
the operation is allowed immediately, and the next plugin is not called.
`group` and `mapping` do not grant. When capability does not grant, the
request follows that issuer's `jwt.onmissing`, or `accjwt.onmissing` when the
issuer did not set one. Groups stay on `Entity.grps`. When
`mapping` is enabled and the issuer exported `jwt.mapfile`, the first matching
rule is applied to `request.name` only after the operation is granted.
Lookups use the handshake account, stored once on the connection.
`Entity.name` is not changed. A miss restores that handshake `request.name`.
A path that returns through `onmissing` is forwarded with the handshake `request.name`.
The rule path is a prefix under each issuer `base_path`: `"path": "/bbockelm"`
with base path `/home` matches `/home/bbockelm` and everything under it. A
rule with no `path` matches any request under the base path. `sub`,
`username`, and `group` are exact. `username` is compared with the handshake
account.

Omitting `capability` (`accjwt.strategy group`, or `authorization_strategy = group`
in a SciTokens file) means a matching scope does not grant. `onmissing`
decides. `accjwt.strategy` overrides the per-issuer `jwt.strategy` attribute.
When neither is set, the default includes `capability`.

### Base path example

A token with `storage.read:/data` and site configuration
`accjwt.basepath /atlas` authorizes reads under `/atlas/data`, not `/data`.

---

## Scope-to-operation mapping

### WLCG scopes

| Scope | Permitted operations |
| ----- | -------------------- |
| `storage.read:<path>` | read, readdir, stat |
| `storage.create:<path>` | exclusive create, mkdir, rename, exclusive insert, stat |
| `storage.modify:<path>` | create, mkdir, rename, insert, update, chmod, stat, delete |
| `storage.stage:<path>` | stage (bring online, pin, release) |
| `storage.poll:<path>` | poll (query staging status) |

`storage.stage` and `storage.poll` are independent: neither implies read or
the other.

### SciTokens scopes

| Scope | Permitted operations |
| ----- | -------------------- |
| `read:<path>` | read, readdir, stat |
| `write:<path>` | create, mkdir, rename, insert, update, chmod, stat, delete |

`write` does not imply `read`; use both scopes on the same path for full access.

### Path semantics

Scopes authorize a **subtree** rooted at the given path:

```
storage.read:/data  →  /data/file1, /data/subdir/file2  ✓
                       /other                            ✗
```

`stat` and `mkdir` on parent directories of an authorized path are also
permitted, consistent with WLCG storage guidance.

---

## Plugin chaining

`XrdAccjwt` can wrap another authorization plugin. Token-based scope
authorization is evaluated first; if it does not grant access, behavior depends
on `accjwt.onmissing`:

```conf
ofs.authorize
ofs.authlib libXrdAccjwt.so
ofs.authlib++ libXrdAcc.so
```

With `onmissing passthrough`, a request this plugin does not authorize falls
through to `libXrdAcc.so` (for example a session with no scope, or a scope
that does not cover the operation).

---

## Complete example

### WLCG storage with tape staging

**`xrootd.cf`:**

```conf
xrootd.seclib libXrdSec.so
xrootd.tls all
xrd.tlsca certdir /etc/grid-security/certificates

sec.protparm jwt -issuer https://iam.example.org
sec.protparm jwt -audience eos-storage
sec.protparm jwt -expiry required
sec.protparm jwt -entity-claim scope
sec.protparm jwt -entity-claim wlcg.ver
sec.protocol jwt

accjwt.basepath /eos/user
accjwt.trace info
accjwt.onmissing deny

ofs.authorize
ofs.authlib libXrdAccjwt.so
```

**Token:**

```json
{
  "wlcg.ver": "1.0",
  "scope": "storage.read:/public storage.modify:/alice storage.stage:/tape"
}
```

**Effective authorization** (with `basepath /eos/user`):

| Token scope | Effective path | Access |
| ----------- | -------------- | ------ |
| `storage.read:/public` | `/eos/user/public/**` | read |
| `storage.modify:/alice` | `/eos/user/alice/**` | write (no read) |
| `storage.stage:/tape` | `/eos/user/tape/**` | stage only |

### SciTokens analysis workflow

**Token** (no `wlcg.ver`):

```json
{
  "scope": "read:/datasets write:/scratch"
}
```

Grants read-only access to `/datasets/**` and full write access to
`/scratch/**`.

---

## Comparison with XrdSciTokens

| | XrdAccjwt | XrdSciTokens |
| - | ----------- | ------------ |
| JWT validation | via `XrdSecJWT::ValidateToken` (login, and `authz=` on an xroot open) | built-in (libscitokens) |
| Configuration | `sec.protocol jwt` + `ofs.authlib` | `ofs.authlib` + `scitokens.cfg` |
| WLCG scopes | yes (`wlcg.ver` detection) | via scitokens-cpp |
| SciTokens scopes | yes (default when no `wlcg.ver`) | yes |
| Tape staging scopes | yes (WLCG only) | partial |
| Issuer/JWKS config | in the authentication plugin | in scitokens.cfg |
| Several `base_path` values | yes (JSON array, any prefix matches) | yes |
| Scope does not cover the operation | `jwt.onmissing`, or `accjwt.onmissing` | `onmissing` |
| `TokenInfo` on an authorized transfer | yes (`s`, `n`, `o`, `r`, `g`). `o` is `Entity.vorg`, the issuer | yes |
| `authorization_strategy` | `accjwt.strategy`, or `jwt.strategy` from the issuer | `capability`, `group`, `mapping` |
| `name_mapfile` | `jwt.mapfile`, written to `request.name` only after a grant | JSON rules, including a path prefix |

`XrdAccjwt` is suited for deployments that already authenticate with
`sec.protocol jwt` and want authorization driven directly
from entity attributes, without a separate SciTokens configuration file.
This plugin grants from storage scopes when `capability` is enabled.
Anything else follows that issuer's `jwt.onmissing`, or `accjwt.onmissing`
when the issuer did not set one. A matching `name_mapfile` rule
sets `request.name` only after that operation is granted. The handshake
account in `Entity.name` stays in place. A miss, and a request that is
not granted, both leave `request.name` as that handshake account.

---

## Building

The plugin is built as part of the XRootD server (`libXrdAccjwt-<version>.so`).
When `libXrdSecjwtCore` is built, Accjwt links it so an xroot `authz=` token
uses the same validator and cache as login.

Unit tests (with `-DENABLE_TESTS=ON`):

```sh
cmake --build . --target xrdaccjwt-unit-tests
./bin/xrdaccjwt-unit-tests
```

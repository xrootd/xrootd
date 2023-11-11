SciTokens Authorization Support for XRootD
==========================================

This ACC (authorization) plugin for the XRootD framework utilizes the [SciTokens
library](https://www.scitokens.org) to validate and extract authorization claims from
a SciToken passed during a transfer.

Configured appropriately, this allows the XRootD server admin to delegate authorization
decisions for a subset of the namespace to an external issuer.  For example, this would
allow LIGO to decide the read/write authorizations for pieces of the LIGO namespace.

Loading the plugin in the xrootd daemon
---------------------------------------

To load the plugin, using the default authorization scheme as a fallback,
add the following lines to your XRootD configuration file:

```
ofs.authorize
ofs.authlib ++ libXrdAccSciTokens.so

# Pass the bearer token to the XRootD authorization framework.
http.header2cgi Authorization authz
```
Note that you will need to configure the default authorization scheme as well.
If SciTokens is the *only* authorization scheme allowed then you can
omit the "++" and all requests are required to present a valid bearer token.

Restart the xrootd daemon.  The SciTokens plugin in the `ofs.authlib` line additionally can take a
parameter to specify the configuration file:

```
ofs.authlib [++] libXrdAccSciTokens.so config=/path/to/config/file
```

If not given, it defaults to `/etc/xrootd/scitokens.cfg`.  Restart the service for new settings to take effect.

The SciTokens plugin has multiple levels of logging output.  To manage these, set:

```
scitokens.trace LEVEL_NAME
```

Valid levels include `error`, `warning`, `info`, `debug`, and `all`.

SciTokens Configuration File
----------------------------

The SciTokens configuration file (default: `/etc/xrootd/scitokens.cfg`) specifies the recognized
issuers and maps them to the XRootD namespace.  It uses the popular INI-format.  Here is an example
entry:

```
[Global]
audience = test_server

[Issuer OSG-Connect]

issuer = https://scitokens.org/osg-connect
base_path = /stash
map_subject = True
default_user = osg
name_mapfile = /path/to/mapfile
```

Duplicate section and settings names (not values) will take the value of the last entry in the file.  For example:

```
[Issuer https://chtc.cs.wisc.edu/icecube]
issuer = https://chtc.cs.wisc.edu/icecube
base_path = /icecube/path1

[Issuer https://chtc.cs.wisc.edu/icecube]
issuer = https://chtc.cs.wisc.edu/icecube
base_path = /icecube/path2
```

Will result in a configuration with `issuer =  https://chtc.cs.wisc.edu/icecube` and `base_path = /icecube/path2`.

Within the `Global` section, the available attributes are:

   - `audience` (optional): A comma separated list of acceptable audiences.  The tokens must have an `aud` attribute
     that exactly matches this value
   - `audience_json` (optional): JSON string or list specifying the acceptable audiences.  This audience option will allow
     commas and spaces within the audience.  `audience_json` takes precedence over `audience`.
   - `onmissing` (optional): The behavior for the plugin when a token is not present or the token does not authorize
     the requested action.  Valid values are:

        - `passthrough`: Invoke the next configured authorization plugin.
        - `allow`: Immediately authorize the request.  This may be useful when the XRootD authorization is supposed to
          always succeed because authorization is implemented in the filesystem (note this plugin may still update
          the internal credential with useful information for the filesystem to make a decision).
        - `deny`: Immediately deny the request.

     If the token is present and valid, then the internal XRootD credential will be populated with any present
     group or issuer information from the token.  The username is only populated if either scope-based mapping or
     the mapfile-based approach is successful.

Each section name specifying a new issuer *MUST* be prefixed with `Issuer`.  Known attributes
are:

   - `issuer` (required): The URI of the token issuer; this must match the value of the corresponding claim int
      the token.
   - `base_path` (required): The paths any token authorizations are relative to; a comma-separated list is permitted.
   - `restricted_path` (optional): Any restrictions on the paths the issuer can authorize *inside* their namespace.  This
      meant to be a mechanism to help with transitions, where the local site storage is setup such that an issuer's
      namespace contains directories that should not be managed by the issuer.
   - `map_subject` (optional): Defaults to `false`; if set to `true`, any contents of the `sub` claim will be copied
      into the Xrootd username.  When combined with the [xrootd-multiuser](https://github.com/bbockelm/xrootd-multiuser)
      plugin, this will allow the xrootd daemon to write out files utilizing the Unix username specified by the VO
      in the token.  Except in narrow use cases, the default of `false` is sufficient.
   - `default_user` (optional): If set, then all authorized operations will be done under the provided username when
      interacting with the filesystem.  This is useful in the case where the administrator desires that all files owned
      by an issuer should be mapped to a particular Unix user account at the site.
   - `username_claim` (optional): Not all issuers put the desired username in the `sub` claim (sometimes the subject is
      set to a de-identified value).  To use an alternate claim as the username, such as `uid`, set this to the desired
      claim name.  If set, it overrides `map_subject` and `default_user`.
   -  `name_mapfile` (options): If set, then the referenced file is parsed as a JSON object and the specified mappings
      are applied to the username inside the XRootD framework.  See below for more information on the mapfile.
   -  `authorization_strategy` (optional): One or more authorizations to use from the token.  Multiple (space separated)
      items may be specified from the following valid values:

         - `capability`: Authorize based on capabilities (e.g., `storage.read:/foo`) from the token.
         - `group`: Pass through the request if there's any group present in the token.
         - `mapping`: Pass through the request if the user mapping was successful.

      For the `group` and `mapping` cases, the username and group are set in the internal XRootD request credential,
      but the final authorization must be done by a subsequent plugin.  The default value is `capability group mapping`.
      *Note*: if `mapping` is present, then a token without a capability may still have authorized actions.


Group- and Scope-based authorization
------------------------------------

WLCG tokens can contain either group- or scope-based attributes.  The scope-based attributes specify a path the user
is allowed to access (relative to one of the base paths).  If a request is permitted via a scope-based attribute, then
it is approved immediately by the plugin.

If there is a group-based attribute, then the contents are copied into XRootD's internal credential.  The plugin does
not necessarily immediately authorize (see the `onmissing` attribute) but rather can be used by a further authorization
plugin.

Mapfile format
--------------

The file specified by the `name_mapfile` attribute can be used to perform identity mapping for a given issuer.
It must parse as valid JSON and may look like this:

```
[
   {"sub": "bbockelm",    "path": "/home/bbockelm", "result": "bbockelm"},
   {"group": "/cms/prod", "path": "/cms",           "result": "cmsprod" comment="Added 1 Sept 2020"},
   {"group": "/cms",                                "result": "cmsuser"},
   {"group": "/cms",                                "result": "atlas"   ignore="Only for testing"}
]
```

That is, we have a JSON list of objects; each object is interpreted as a rule.  For an incoming request to match a rule,
each present attribute must evaluate to true.  In this case, the value of the `result` key is populated as the username
in the XRootD internal credential.

The enumerated keys are:
   - `sub`: True if the `sub` claim in the token matches the value in the mapfile (case-sensitive comparison).
   - `username`: True if the username in the token (the claim specifying the username is configurable, controlled by the
     `username_claim` variable in the issuer config; default is `sub`) matches the value in the mapfile (case-sensitive
     comparison).
   - `path`: True iff the value of the attribute matches (case-sensitive) the prefix of the (normalized) requested path.
     For example, if the issuer's base path is `/home`, the operation is accessing `/home/bbockelm/foo`, and the path in
     the rule is `/bbockelm`, then this attribute evaluates to `true`.  Note the path value and the requested path must
     be normalized; if presented with `/home//bbockelm/`, then this is treated as if `/home/bbockelm` was given.
   - `group`: Case-sensitive match against one of the groups in the token.
   - `ignore`: If present (regardless of the value), the rule is ignored.
   - `comment`: Ignored; reserved for adding comments from the administrator.

Unknown keys are ignored.


VOMS Mapping
============

The VOMS plugin can now populate the XRootD session's `name` attribute from a
mapping file (the "voms-mapfile").  Filesystems which rely on the username
in addition to the XRootD authorization can utilize this name to make authorization
and file ownership decisions.

Note the plugins have the following precedence for the `name` attribute:

- Explicit entry in the grid-mapfile.
- Entry in the voms-mapfile.
- Default auto-generated name for the grid mapfile.

Administrators may desire to disable the auto-generated name as it likely does
not match any Unix username on the system.

Configuration
-------------

There are two configuration options that control the plugin:

```
voms.mapfile FILENAME
```

Enables the mapping functionality and uses the file at FILENAME as the voms-mapfile.
The mapfile is reloaded every 30 seconds; the daemon does not need to be restarted
to pick up changes.

```
voms.trace [none|all|debug|info|warning|error]+
```

Enable debugging of the VOMS mapfile logic.  Options are additive and multiple can be
given.

Format and Matching Details
---------------------------

The file format ignores empty lines; a line beginning with the hash (`#`) are considered
comments and ignored.

Otherwise, each line specifies a mapping from an expression to a Unix username in the
following form:

```
"EXPRESSION" USERNAME
```

If the session has a VOMS FQAN matching EXPRESSION then the session's name will be set
to USERNAME.

Examples of the EXPRESSION include:

```
/cms/Role=production/Capability=NULL
/atlas/usatlas/Role=pilot/Capability=NULL
```

Expressions may also have wildcards (`*`) present.  The wildcard can serve as
two roles:

- If the expression ends with `/*`, then any remaining portion of the attribute
  is matched.  For example, `/cms/*` matches `/cms/Role=NULL/Capability=NULL` and
  `/cms/uscms/Role=pilot/Capability=NULL`.
- If the wildcard is inside a path hierarchy, it allows any character inisde the
  path.  For example, `/fermilab/*/Role=pilot/Capability=NULL` matches both
  `/fermilab/dune/Role=pilot/Capability=NULL` and `/fermilab/des/Role=pilot/Capability=NULL`
  but not `/fermilab/Role=pilot/Capability=NULL`.

Several escape sequences are supported within the expression:

- `\'`: a single quote character (`'`).
- `\"`: a double quote character (`"`).
- `\\`: a backwards slash (`\`).
- `\/`: a forward slash that is not a path separator (`/`)
- `\f`: a formfeed
- `\n`: a newline
- `\r`: a carriage return
- `\t`: a tab character.

The use of these escape sequences are discouraged as it's unclear whether other software
is able to safely handle them.  Unicode and extended 8-bit ASCII are not supported at this
time.

Note, as is tradition, the name of the VO in the VOMS FQAN must match the first group name.
That is, if the `cms` VO issues a FQAN of the form `/atlas/Role=pilot/Capability=NULL` then
the FQAN is ignored.


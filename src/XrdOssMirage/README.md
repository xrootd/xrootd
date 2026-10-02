# XrdOssMirage

XrdOssMirage is a server-side plugin that simulates the existence of files on an XRootD server. It requires no storage: any uploaded file content is discarded, while only the file size is retained.

When reading a file, the plugin supports three different modes: returning arbitrary garbage data, generating a repeated character pattern, or generating a repeated string pattern.

It is also possible to upload an empty or very small file and then truncate it to simulate a huge file on the server side, avoiding the need for lengthy data transfers.

No data is persisted: the entire filesystem exists only in the volatile memory.

The plugin runs in one of two modes:

- **Standalone mode**: the plugin serves the whole namespace.
- **Stacked mode**: the plugin serves only one path prefix. Other paths go to the plugin below it (for example, the default disk storage).

## Configuration

### Standalone mode

The plugin needs no configuration parameter.

To enable it, add the following line to the configuration file:

```
ofs.osslib libXrdOssMirage.so
```

### Stacked mode

To stack the plugin on top of another OSS plugin, use `++` and give the mirage path as the plugin parameter:

```
ofs.osslib ++ libXrdOssMirage.so /mirage_test/
```

The mirage path is mandatory. The server does not start in these cases:

| Configuration | Result |
|---|---|
| No mirage path | Error: `no mirage path given in the plugin parameters` |
| Mirage path `/` | Error: `mirage path '/' is not allowed` (it covers the whole namespace) |

The plugin reads only the first word of the parameter.

### Summary of setups

| Mode | Extended options | Configuration |
|---|---|---|
| Standalone | No | `ofs.osslib libXrdOssMirage.so` |
| Standalone | Yes | `ofs.osslib libXrdOssMirage.so`<br>`ofs.xattrlib libXrdOssMirage.so` |
| Stacked | No | `ofs.osslib ++ libXrdOssMirage.so /mirage_test/` |
| Stacked | Yes | `ofs.osslib ++ libXrdOssMirage.so /mirage_test/`<br>`ofs.xattrlib ++ libXrdOssMirage.so` |

Full example of a stacked configuration with extended options:

```
all.export /
oss.localroot /data

ofs.osslib   ++ libXrdOssMirage.so /mirage_test/
ofs.xattrlib ++ libXrdOssMirage.so
```

## Stacked mode behavior

### Path routing

A path belongs to the mirage plugin when it starts with the mirage path. The plugin always appends a trailing `/` to the mirage path. The match covers one directory tree only. The plugin routes each operation by its path:

- **Mirage path**: the plugin handles the operation in memory. It stores no data and no file on disk.
- **Other path**: the plugin forwards the operation unchanged to the stacked plugin.

The routing applies to these operations: create, open, stat, mkdir, rmdir, rename, truncate, unlink, chmod, and directory open.

Example: mirage path `/mirage` becomes `/mirage/`. The path `/mirage/under_mirage` matches. The path `/mirage2/file` does not match.

Example (mirage path `/mirage_test/`):

```
$ echo 'stored on disk' | xrdcp - root://localhost//diskfile
$ echo 'discarded content' | xrdcp - root://localhost//mirage_test/memfile
$ ls /data
diskfile
$ xrdfs root://localhost/ stat /mirage_test/memfile
...
Size:   18
```

### Content returned when reading

In stacked mode, a read of a mirage file returns the pattern `0` (the character `0` repeated up to the file size) when no `pattern` attribute is set. The plugin does not return arbitrary memory content in this mode.

```
$ head -c 10 /dev/zero | xrdcp -f - root://localhost//mirage_test/memfile
$ xrdfs root://localhost/ cat /mirage_test/memfile
0000000000
```

The `pattern` attribute overrides this default (see [Custom generated pattern for file contents](#custom-generated-pattern-for-file-contents)).

In standalone mode, the plugin keeps the original behavior and can return arbitrary memory content.

A file outside the mirage path returns its exact stored content.

### Directories

- A directory outside the mirage path can be listed as usual.
- A directory listing under the mirage path returns an error.

### Physical file name

The mirage plugin does not add `oss.localroot` to a mirage path. The physical name of a mirage path is equal to its logical name. A path outside the mirage path goes to the stacked plugin, which applies its own name translation.

## Extendend options

If extended configuration options are required, the plugin also supports settings such as:

- custom return code when opening files
- custom return code when closing files
- custom return code when reading files (optionally at specific offset)
- custom return code when writing files (optionally at specific offset)
- configurable content data patterns

To use the extended options, the extended attribute lib must be enabled with:

```
ofs.xattrlib libXrdOssMirage.so
```

In stacked mode, use `++` so that the attribute plugin stacks on the current one:

```
ofs.xattrlib ++ libXrdOssMirage.so
```

In stacked mode, the plugin routes each attribute operation by path:

- **Mirage path**: the attribute is a mirage option, as described in this document. The mirage plugin must have created at least one file before it can accept attributes.
- **Other path**: the stacked attribute plugin stores the attribute as an ordinary attribute. The name has no special meaning. For example, `open.return_code` or `pattern` on a real file does not inject an error and does not change the content.
- **Other path that does not exist**: the operation fails, because the stacked plugin has no file for the attribute.

## Usage

### Creating an empty file and truncating its size

The example below shows how to create a zero byte file to avoid unnecessary data transfer, since the server ignores file contents, and then expand it to 1 GB by truncating its size.

```
head -c 0 /dev/zero | xrdcp - root://localhost//remotefile
xrdfs root://localhost/ truncate /remotefile 1073741824
```

Example:

```
$ head -c 0 /dev/zero | xrdcp -f - root://localhost//remotefile
[0B/0B][100%][==================================================][0B/s]  
$ xrdfs root://localhost/ truncate /remotefile 1073741824
$ xrdfs root://localhost/ stat /remotefile           

Path:   /remotefile
Id:     25769803871
Size:   1073741824
MTime:  1970-01-01 00:00:00
Flags:  4 (Other)
```

## Usage of extended options

The extended options are implemented through the extended attributes plugin. This enables additional functionality useful for testing and development without requiring manual file setup or custom implementations.

Extended attributes can only be applied to existing files and remain preserved even if the file is overwritten.

Attributes can be deleted or reset to their original values.

```
xrdfs root://localhost/ xattr /remotefile del open.return_code
xrdfs root://localhost/ xattr /remotefile del pattern
...
```

When a file is deleted, all associated attributes are removed as well.

```
xrdfs root://localhost/ rm /remotefile
```

### Custom return code when opening a file

To simulate a custom error when opening a file, a fake extended attribute `open.return_code` can be set.

```
xrdfs root://localhost/ xattr /remotefile set open.return_code=CODE
```

Example:

```
$ xrdfs root://localhost/ xattr /remotefile set open.return_code=12
$ xrdcp root://localhost//remotefile localfile 
[0B/0B][100%][==================================================][0B/s]  
Run: [ERROR] Server responded with an error: [3008] Unable to open /remotefile; cannot allocate memory (source)
$ xrdcp -f localfile root://localhost//remotefile
[0B/0B][100%][==================================================][0B/s]  
Run: [ERROR] Server responded with an error: [3008] Unable to open /remotefile; cannot allocate memory (destination)
```

### Custom return code when closing a file

To simulate a custom error when closing a file, a fake extended attribute `close.return_code` can be set.

```
xrdfs root://localhost/ xattr /remotefile set close.return_code=CODE
```

Example:

```
$ xrdfs root://localhost/ xattr /remotefile set close.return_code=12
$ xrdcp root://localhost//remotefile localfile
[0B/0B][100%][==================================================][0B/s]  
Run: [ERROR] Server responded with an error: [3008] Unable to close /remotefile; cannot allocate memory (source)
```

### Custom return code when reading a file at a specific position

To simulate a custom error when reading a file, the fake extended attribute `read.return_code` can be set. The `read.return_position` attribute allows the error to be triggered at a specific read offset; if it is not set, the operation fails immediately at position 0.

```
xrdfs root://localhost/ xattr /remotefile set read.return_code=CODE
xrdfs root://localhost/ xattr /remotefile set read.return_position=POSITION
```

Example:

```
$ xrdfs root://localhost/ xattr /remotefile set read.return_code=12
$ xrdfs root://localhost/ xattr /remotefile set read.return_position=1000000000
$ xrdcp root://localhost//remotefile localfile
[568MB/1024MB][ 55%][===========================>                      ][568MB/s]
Run: [ERROR] Server responded with an error: [3008] Unable to read /remotefile; cannot allocate memory (source)
```

### Custom return code when writing a file at a specific position

To simulate a custom error when writing to a file, the fake extended attribute `write.return_code` can be set. The `write.return_position` attribute allows the error to be triggered at a specific write offset; if it is not set, the operation fails immediately at position 0.

This option is only available when overwriting an existing file, since the extended attributes must already be present on the file before writing.

```
xrdfs root://localhost/ xattr /remotefile set write.return_code=CODE
xrdfs root://localhost/ xattr /remotefile set write.return_position=POSITION
```

Example:

```
$ xrdfs root://localhost/ xattr /remotefile set write.return_code=12
$ xrdfs root://localhost/ xattr /remotefile set write.return_position=1000000000
$ xrdcp -f localfile root://localhost//remotefile                      
[648MB/1024MB][ 63%][===============================>                  ][324MB/s]  
Run: [ERROR] Server responded with an error: [3008] Unable to write /remotefile; cannot allocate memory (destination)
```

### Custom generated pattern for file contents

Files “stored” by this plugin contain no actual data. When a file is downloaded, arbitrary memory content may be returned as fake data.

The plugin can also be configured to generate deterministic output instead. One option is to return a full string pattern, limited by the maximum size of a `std::string` in the system. Another option is to repeat a single character indefinitely.

Both approaches introduce performance overhead compared to returning arbitrary memory data, with the full string pattern being the most expensive.

```
xrdfs root://localhost/ xattr /remotefile set pattern=PATTERN
```

Example:

```
$ head -c 100 /dev/zero | xrdcp -f - root://localhost//remotefile
[100B/0B][100%][==================================================][100B/s]
$ xrdfs root://localhost/ xattr /remotefile set pattern=abcde123
$ xrdfs root://localhost/ cat /remotefile abcde123abcde123abcde123abcde123abcde123abcde123abcde123abcde123abcde123abcde123abcde123abcde123abcd
$ xrdfs root://localhost/ xattr /remotefile set pattern=1       
$ xrdfs root://localhost/ cat /remotefile
1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111
```
# XrdOssMirage

XrdOssMirage is a server-side plugin that simulates the existence of files on an XRootD server. It requires no storage: any uploaded file content is discarded, while only the file size is retained.

When reading a file, the plugin supports three different modes: returning arbitrary garbage data, generating a repeated character pattern, or generating a repeated string pattern.

It is also possible to upload an empty or very small file and then truncate it to simulate a huge file on the server side, avoiding the need for lengthy data transfers.

No data is persisted: the entire filesystem exists only in the volatile memory.

## Configuration

The plugin is a runtime-only plugin that requires minimal configuration.

To configure it, simply add the following line to the configuration file:

```
ofs.osslib libXrdOssMirage.so
```

## Extendend options

If extended configuration options are required, the plugin also supports settings such as:

- custom return code when opening files
- custom return code when reading files (optionally at specific offset)
- custom return code when writing files (optionally at specific offset)
- configurable content data patterns

To use the extended options, the extended attribute lib must be enabled with:

```
ofs.xattrlib libXrdOssMirage.so
```

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
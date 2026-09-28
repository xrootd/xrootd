## XRootD Python Bindings

This is a set of simple but pythonic bindings for XRootD. It is designed to make
it easy to interface with the XRootD client, by writing Python instead of having
to write C++.


## Standard file objects

`client.open()` provides ordinary Python file methods, including `read`, `write`,
`seek`, `tell`, text encoding, and context management. It maps XRootD failures to
standard `OSError` subclasses while preserving the native status as
`error.xrootd_status`. The original `client.File` API remains available when
explicit offsets, status tuples, or callbacks are useful.

Before:

```python
from XRootD import client
from XRootD.client.flags import OpenFlags

file = client.File()
status, _ = file.open('root://host//data/file', OpenFlags.READ)
client.raise_on_error(status)
try:
    status, data = file.read(offset=0, size=1024)
    client.raise_on_error(status)
finally:
    file.close()
```

After:

```python
from XRootD import client

with client.open('root://host//data/file', 'rb') as file:
    data = file.read(1024)
```

Text mode also supports `encoding`, `errors`, and `newline` arguments.

## Filesystem path helpers

`FileSystem` also offers `exists`, `is_file`, `is_dir`, `listdir`, and
`makedirs`. These methods return Python values and raise `OSError` subclasses
on failures. The existing methods continue to return status tuples.

Before:

```python
from XRootD import client
from XRootD.client.flags import DirListFlags, MkDirFlags

fs = client.FileSystem('root://host')
status, listing = fs.dirlist('/data', flags=DirListFlags.NONE)
client.raise_on_error(status)
names = [entry.name for entry in listing]
status, _ = fs.mkdir('/data/new/subdir', flags=MkDirFlags.MAKEPATH)
client.raise_on_error(status)
```

After:

```python
from XRootD import client

fs = client.FileSystem('root://host')
names = fs.listdir('/data')
fs.makedirs('/data/new/subdir', exist_ok=True)
if fs.is_file('/data/file'):
    print('file exists')
```

For listings that need metadata, `scandir()` returns entries with remote
`path`, `statinfo`, `size`, `is_file()`, and `is_dir()`. `checksum()` parses a
server checksum into an `(algorithm, value)` tuple. `remove_tree()` removes a
directory in postorder and returns file, directory, and byte counts:

```python
for entry in fs.scandir('/data'):
    if entry.is_file():
        print(entry.path, entry.size)

info = fs.stat_info('/data/file')  # metadata or a standard OSError subclass
algorithm, value = fs.checksum('/data/file')
fs.unlink('/data/obsolete', missing_ok=True)
removed = fs.remove_tree('/data/obsolete')
print(removed.files_removed, removed.size_removed)

result = client.CopyProcess.copy_one(
    'root://source//data/file', 'root://target//data/file',
    force=True, mkdir=True, cptimeout=120)
```

These helpers raise standard `OSError` subclasses on failure; the lower-level
status-tuple methods and configurable multi-job `CopyProcess` remain available.
The core helpers use syntax compatible with Python 3.6 for AlmaLinux 8.


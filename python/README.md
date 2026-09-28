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

## Asyncio and fsspec (draft)

The classic bindings, synchronous Python helpers and callback API remain
installable and usable with Python 3.6, including AlmaLinux 8's system Python.
The optional `XRootD.client.aio`, `asyncstream` and `fsspec` interfaces require
Python **3.11 or later**. Importing them on an older interpreter raises a clear
`ImportError` without preventing subsequent use of the classic bindings.
Importing `XRootD.client` does not load these optional modules or fsspec.

Both package entry points declare `python_requires >= 3.6`. The adapter is
included in the bindings; install `fsspec>=2024.2.0` separately to use it on
Python >=3.11. All packaged modules retain Python 3.6-compatible syntax so
installation and byte-compilation work there. Modern asyncio APIs are used after the version
check, which runs before importing optional dependencies.

`XRootD.client.aio` provides awaitable file and filesystem operations backed by
XrdCl callbacks. It does not use a thread pool for remote I/O. Calls made through
the low-level `aio.File` interface must use bounded reads; cancellation stops
waiting for a request but does not abort an already submitted XrdCl operation.

Before, an asyncio caller had to bridge the existing callback API manually
(shown for a file that is already open):

```python
import asyncio
from XRootD import client

loop = asyncio.get_running_loop()
future = loop.create_future()

def on_read(status, data, hosts):
    def finish():
        try:
            client.raise_on_error(status)
        except Exception as error:
            future.set_exception(error)
        else:
            future.set_result(data)
    loop.call_soon_threadsafe(finish)

client.raise_on_error(file.read(offset=0, size=1024, callback=on_read))
data = await future
```

After, the adapter handles callback completion and status errors:

```python
from XRootD.client.aio import File

file = await File().open('root://host//path/to/file')
async with file:
    data = await file.read(0, 1024)
```

For ordinary sequential file access, `aio.open` manages opening, the cursor,
and closing. It works independently of fsspec and raises standard `OSError`
subclasses, with the native status available as `error.xrootd_status`:

```python
from XRootD.client import aio

async with aio.open('root://host//path/to/file', 'rb', timeout=30) as file:
    header = await file.read(1024)
    await file.seek(0)
    async for line in file:
        process(line)  # bytes, including the trailing newline

async with aio.open('root://host//path/to/output', 'wb') as file:
    await file.write(b'hello\n')
    await file.flush()
```

The binary modes `rb`, `wb`, `xb`, `ab`, and their `+` variants are supported.
`read()`, `readline()`, `readinto()`, `write()`, `seek()`, `truncate()`, `flush()`,
and `close()` are awaitable; `tell()` and `closed` report local state. You may
also use `file = await aio.open(url)` and later `await file.aclose()`.
`read()` without a size reads to EOF in bounded native requests and assembles
the result in memory. Use `iter_chunks(size)` or line iteration for large files.

One stream serializes operations on its shared cursor. Use `read_at(offset,
size)` for concurrent reads at independent positions on the same handle; these
leave the cursor and any read-ahead buffer unchanged. Closing waits for all
positioned reads. Concurrent reads and writes do not provide snapshot isolation. Cancelling a stream operation
waits for the current native request to complete before releasing its lock;
cancelling an open closes any handle obtained by the pending request. Context
exit and `close()` also finish cleanup when cancelled. This prevents closing a
handle while its read or write is still pending. Cancellation does not roll
back writes, and may advance the cursor. Set `timeout` to bound native waits.
Append obtains the current EOF before writing; concurrent writers on separate
handles do not have an atomic append guarantee. These stream interfaces require
Python 3.11 or later.

#### Concurrent ranges and bounded-memory iteration

Previously, concurrent ranges required manually managing the lifetime of a
low-level `aio.File`, supplying offsets to every read, and ensuring pending
callbacks completed before close. The stream now owns that lifecycle:

```python
import asyncio
from XRootD.client import aio

async def read_ranges(url):
    async with aio.open(url, timeout=10) as file:
        header = await file.read(4)
        first, second = await asyncio.gather(
            file.read_at(1024, 65536),
            file.read_at(1024 * 1024, 65536),
        )
        assert file.tell() == 4
        return header, first, second
```

For sequential processing, blocks are read only as the consumer requests them:

```python
async def process_file(url):
    async with aio.open(url, timeout=10) as file:
        async for block in file.iter_chunks(1024 * 1024):
            await consume(block)
```

Each block is at most the requested size, and the last block may be shorter.
Breaking out of iteration submits no further reads; the enclosing `async with`
closes the stream. `read_at` assembles its requested range in memory, so use
bounded ranges and limit the number of concurrent tasks for large workloads.
These methods are also available on fsspec's `open_async` files.

For a complete command-line example, including timeout handling and optional
`TaskGroup`, see
[`examples/async_streams/read.py`](examples/async_streams/read.py). It requires
Python 3.11 or later and uses `asyncio.run`. Public async stream methods
carry type annotations, and the package includes a `py.typed` marker.

#### Cancellation and resource ownership

The sequential streams returned by `aio.open` and fsspec's `open_async`
share the same implementation and cancellation contract:

| Operation | What cancellation means |
| --- | --- |
| Waiting for the cursor lock | No new request is submitted. |
| Opening | Wait for completion, then close a successfully opened handle. |
| Reading or writing | Finish the current native request before releasing the handle. Earlier chunks may already have advanced the cursor; writes are not rolled back. |
| Closing or leaving `async with` | Complete cleanup even if cancellation is requested again. |
| Cached fsspec reads | Retain the cache reference until native completion; vector batches also drain before an error is propagated. |

`asyncio.wait_for` cancels the Python task. It cannot abort an XrdCl request,
so cleanup may outlast the asyncio deadline. A positive native `timeout`
bounds individual network requests, not a whole multi-request transfer or
its cleanup. `timeout=0` uses XrdCl's configured default.
`asyncio.wait_for` waits for cancellation cleanup to finish on supported
interpreters. `asyncio.timeout` and `TaskGroup` follow the same native resource
ownership requirements.

The low-level `aio.request` and `aio.File` APIs remain available for callers
which manage native request ownership themselves. Their cancellation stops
waiting immediately; arguments are retained until the callback arrives, but
callers must not close or reuse a handle with outstanding requests. Prefer
`aio.open` for automatic ownership and cleanup.

The filesystem also provides awaitable Python-style helpers. Previously each
caller had to inspect stat flags, listing responses, and native errors:

```python
status, listing = client.FileSystem('root://host').dirlist('/data')
client.raise_on_error(status)
names = [entry.name for entry in listing]
```

With the asynchronous helpers:

```python
fs = aio.FileSystem('root://host')
names = await fs.listdir('/data')
for entry in await fs.scandir('/data'):
    if entry.is_file():
        print(entry.path, entry.size)
await fs.makedirs('/data/output', exist_ok=True)
info = await fs.stat_info('/data/input')
exists = await fs.exists('/data/input')
algorithm, digest = await fs.checksum('/data/input', algorithm='adler32')
await fs.unlink('/data/temporary', missing_ok=True)
```

`exists`, `is_file`, and `is_dir` return false for missing paths and propagate
permission and connection errors. `listdir`, `scandir`, `makedirs`, `checksum`,
and `unlink` use standard `OSError` subclasses. The original awaitable methods
such as `stat`, `dirlist`, and `rm` retain their native response objects and
XRootD exception classes. Cancelling filesystem operations stops waiting;
an already submitted mutation may still finish on the server.

To use the optional `root` fsspec implementation on Python >=3.11, install
fsspec alongside the bindings:

```sh
python -m pip install xrootd 'fsspec>=2024.2.0'
```

The adapter is included in the bindings; fsspec is an optional dependency
installed separately. The same class supports normal synchronous fsspec
methods and asynchronous calls:

```python
from XRootD.client.fsspec import XRootDFileSystem

fs = XRootDFileSystem(hostid='host', asynchronous=True)
data = await fs._cat_file('/path/to/file', start=0, end=1024)
async with await fs.open_async('/path/to/file', 'rb') as file:
    first_kib = await file.read(1024)
```

Remote open, stat, read, write, list, and namespace operations submit native
XrdCl requests with callbacks. Completion moves from an XrdCl thread to the
asyncio loop with `call_soon_threadsafe`; the loop does not wait in a worker
thread for remote I/O. Local files used by fsspec upload/download run through
an executor. The ordinary `client.open()` and fsspec `fs.open()` methods
are synchronous and should not be used directly inside an event loop. Python
argument handling, request submission, and result handling still run on the
calling thread, so this does not promise zero event-loop latency.

The fsspec adapter now batches scattered ranges through XRootD vector reads,
reuses bounded read handles, and can locate an alternate source if an open fails.
It also supports common metadata, `touch`, `chmod`, checksum queries, append,
and in-place updates. Cached idle read handles are closed after their TTL, and
`invalidate_cache(path)` discards listings and read handles after another client
changes a file. Async callers can await `invalidate_cache_async(path)` for
completion. Downloads accept `chunk_size` as in `fsspec-xrootd`. For synchronous
callers:

```python
fs = XRootDFileSystem(hostid='host')
parts = fs.cat_ranges(['/data/file'] * 2, [0, 4096], [1024, 5120])
with fs.open('/data/file', 'rb') as file:
    header = file.read(1024)
fs.close()
```

Permission changes drain the native request and invalidate this instance's
cached listings and read handles before propagating cancellation, including
when the remote outcome is uncertain. Server authorization still determines
the effect on reads that are already active.

Alternate sources use the native client's authentication and redirect behavior.
`valid_sources` filters located hostnames; it does not confine later redirects
or isolate credentials. Set `locate_all_sources=False` to disable this adapter's
alternate-source lookup, and apply native redirect/authentication policy when
endpoint confinement is required.

The optional fsspec adapter requires Python 3.11 or later, including its
synchronous methods, because it shares the modern async implementation. Its
minimum fsspec dependency remains `fsspec>=2024.2.0`. The classic bindings and
synchronous helpers retain Python 3.6 support for AlmaLinux 8.

`fsspec-xrootd` provides most of those sync operations already, including
vector reads, but its `open_async` returns a synchronous file object. Here,
`open_async` returns a file with awaitable `read`, `write`, `seek`, and `close`.
The native adapter does not register the `root` protocol automatically. To use
it through `fsspec.open()` or an application that selects filesystems by URL,
choose it explicitly before opening a `root://` URL:

```python
import fsspec
from XRootD.client.fsspec import XRootDFileSystem

fsspec.register_implementation('root', XRootDFileSystem, clobber=True)
```

This avoids replacing `fsspec-xrootd` for other applications merely because
XRootD is installed. Cancellation of an already submitted native request
still does not abort the XrdCl operation.


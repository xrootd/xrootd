#!/usr/bin/env python3
"""Read remote ranges and optionally hash a stream using native callbacks.

Python 3.11+:
    python read.py root://host//data/file --offset 0 --offset 1048576
Python 3.11+ structured concurrency:
    python read.py root://host//data/file --task-group --stream

Only reads are performed. The stream option reads the entire file.
"""

import argparse
import asyncio
import hashlib
import sys

from XRootD.client import aio


async def inspect_file(args):
    async with aio.open(args.url, timeout=args.timeout) as file:
        header = await file.read(4)
        print('Header:', repr(header))
        offsets = args.offset or [0, 1048576]
        if args.task_group:
            async with asyncio.TaskGroup() as group:
                tasks = [group.create_task(file.read_at(offset, args.length))
                         for offset in offsets]
            blocks = [task.result() for task in tasks]
        else:
            blocks = await asyncio.gather(
                *(file.read_at(offset, args.length) for offset in offsets),
                return_exceptions=True)
            for block in blocks:
                if isinstance(block, BaseException):
                    raise block
        for offset, block in zip(offsets, blocks):
            print('Offset %d: %d bytes' % (offset, len(block)))
        print('Sequential cursor:', file.tell())  # unchanged by read_at
        if args.stream:
            await file.seek(0)
            digest = hashlib.sha256()
            async for block in file.iter_chunks(args.chunk_size):
                digest.update(block)
            print('SHA256:', digest.hexdigest())


async def main(args):
    work = asyncio.create_task(inspect_file(args))
    try:
        await asyncio.wait_for(work, args.deadline)
    except asyncio.TimeoutError as error:
        # On modern Python, asyncio.TimeoutError aliases the built-in class
        # also used for native ETIMEDOUT. A failed task needs no cancellation.
        if work.done() and not work.cancelled():
            print(str(error), file=sys.stderr)
            return 1
        # Cancellation cleanup can outlast the asyncio deadline: a native
        # request is drained before closing, using its own timeout.
        try:
            await work
        except asyncio.CancelledError:
            pass
        print('Deadline expired; stream cleanup completed.', file=sys.stderr)
        return 1
    except OSError as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('url')
    parser.add_argument('--offset', type=int, action='append')
    parser.add_argument('--length', type=int, default=65536)
    parser.add_argument('--chunk-size', type=int, default=1024 * 1024)
    parser.add_argument('--timeout', type=int, default=10,
                        help='native timeout per request, in seconds')
    parser.add_argument('--deadline', type=float, default=60,
                        help='asyncio deadline for the whole operation')
    parser.add_argument('--stream', action='store_true',
                        help='read and hash the entire file')
    parser.add_argument('--task-group', action='store_true')
    args = parser.parse_args()
    if min(args.length, args.chunk_size, args.timeout, args.deadline) <= 0:
        parser.error('sizes and timeouts must be positive')
    if any(offset < 0 for offset in args.offset or []):
        parser.error('offsets must be nonnegative')
    result = asyncio.run(main(args))
    sys.exit(result)

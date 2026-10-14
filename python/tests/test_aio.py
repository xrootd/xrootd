"""Asyncio bridge checks, including one real server round trip."""

import asyncio
import threading

import pytest

from XRootD import client
from XRootD.client import aio
from XRootD.client.flags import AccessMode, OpenFlags
from XRootD.client.responses import XRootDNotFoundError, XRootDStatus
from env import smallfile


def run_async(coroutine):
    loop = asyncio.new_event_loop()
    try:
        return loop.run_until_complete(coroutine)
    finally:
        loop.close()


def status(ok=True):
    return XRootDStatus({
        'ok': ok, 'code': 0 if ok else XRootDStatus.errNotFound,
        'errno': 0, 'message': 'missing' if not ok else 'ok',
    })


def test_request_completes_on_the_event_loop():
    callback_thread = []

    def operation(callback):
        def finish():
            callback_thread.append(threading.get_ident())
            callback(status(), b'data', [])

        threading.Timer(0.02, finish).start()
        return status()

    async def run():
        loop_thread = threading.get_ident()
        result, _ = await asyncio.gather(
            aio.request(operation), asyncio.sleep(0.005, result='tick'))
        assert result == b'data'
        assert callback_thread != [loop_thread]

    run_async(run())


def test_request_reports_submission_and_completion_errors():
    def rejected(callback):
        return status(False)

    def failed(callback):
        threading.Timer(0.01, callback,
                        args=(status(False), None, [])).start()
        return status()

    async def run():
        with pytest.raises(XRootDNotFoundError):
            await aio.request(rejected)
        with pytest.raises(XRootDNotFoundError):
            await aio.request(failed)

    run_async(run())


def test_cancelled_waiter_ignores_late_completion():
    release = threading.Event()
    finished = threading.Event()

    def operation(callback):
        def finish():
            release.wait(timeout=2)
            callback(status(), b'late', [])
            finished.set()

        threading.Thread(target=finish, daemon=True).start()
        return status()

    async def run():
        task = asyncio.ensure_future(aio.request(operation))
        await asyncio.sleep(0)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task
        release.set()
        loop = asyncio.get_event_loop()
        assert await loop.run_in_executor(None, finished.wait, 2)
        await asyncio.sleep(0)

    run_async(run())


def test_file_round_trip_with_native_callbacks():
    async def run():
        mode = AccessMode.UR | AccessMode.UW
        file = aio.File()
        await file.open(smallfile, OpenFlags.DELETE, mode)
        assert await file.write(b'async data', 0) == 10
        await file.close()
        await file.open(smallfile, OpenFlags.READ)
        assert await file.read(0, 10) == b'async data'
        await file.close()
        fs = aio.FileSystem(client.URL(smallfile).protocol + '://' +
                            client.URL(smallfile).hostid + '/')
        assert (await fs.stat('/tmp/spam')).size == 10
        assert list(await fs.locate('/tmp/spam', OpenFlags.PREFNAME))
        await fs.rm('/tmp/spam')

    run_async(run())


def test_request_rejects_callback_override_and_bounded_reads():
    async def run():
        with pytest.raises(TypeError, match='manages the callback'):
            await aio.request(None, callback=None)
        file = aio.File(native=object())
        for size in [0, -1, 0x100000000]:
            with pytest.raises(ValueError, match='bounded'):
                await file.read(0, size)

    run_async(run())


def test_request_ignores_duplicate_callback_and_closed_loop():
    callbacks = []
    failures = []

    def operation(callback):
        callbacks.append(callback)
        callback(status(), b'first', [])
        callback(status(False), None, [])
        return status()

    loop = asyncio.new_event_loop()
    loop.set_exception_handler(lambda loop, context: failures.append(context))
    try:
        assert loop.run_until_complete(aio.request(operation)) == b'first'
        loop.run_until_complete(asyncio.sleep(0))
    finally:
        loop.close()
    # A native thread may finish after a low-level waiter's loop has closed.
    callbacks[0](status(), b'late', [])
    assert not failures


def test_low_level_context_closes_only_open_files():
    async def run():
        calls = []

        class Native:
            opened = False

            def is_open(self):
                return self.opened

            def close(self, timeout, callback):
                calls.append(timeout)
                self.opened = False
                callback(status(), None, [])
                return status()

        native = Native()
        async with aio.File(native=native):
            pass
        assert not calls
        native.opened = True
        async with aio.File(native=native):
            pass
        assert calls == [0]

    run_async(run())

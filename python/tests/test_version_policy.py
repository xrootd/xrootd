"""Classic bindings remain usable when optional modern APIs are rejected."""

import asyncio
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys

import pytest

from XRootD import client


MODULES = ['aio', 'asyncstream', 'fsspec']
OLDER_VERSIONS = [(3, 6), (3, 8), (3, 9), (3, 10)]


def run_python(source, *args):
    # CTest runs against the installed wheel; propagate that same import path
    # to the fresh interpreter instead of accidentally testing another install.
    environ = dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path))
    result = subprocess.run([sys.executable, '-c', source] + list(args),
                            env=environ, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, universal_newlines=True,
                            timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr


def test_import_policy_preserves_classic_bindings():
    # Use one fresh interpreter for all import checks. Loading the native
    # libraries can be slow on macOS runners; repeating it for every rejected
    # import can exhaust CTest's deadline before the assertions finish.
    run_python('''
import importlib
import json
import sys
from XRootD import client
for name in ('_asyncio', 'aio', 'asyncstream', 'fsspec'):
    assert 'XRootD.client.' + name not in sys.modules
assert callable(client.open)
assert client.File() is not None
assert client.FileSystem('root://localhost') is not None
assert 'fsspec' not in sys.modules
runtime_version = sys.version_info
modules, versions = json.loads(sys.argv[1])
for version in versions:
    for module in modules:
        name = 'XRootD.client.' + module
        context = '%s on Python %s' % (name, version)
        assert name not in sys.modules, context
        sys.version_info = tuple(version)
        try:
            try:
                importlib.import_module(name)
            except ImportError as error:
                assert 'require Python >= 3.11' in str(error), context
                assert 'classic XRootD.client bindings' in str(error), context
            else:
                raise AssertionError('unsupported import: ' + context)
        finally:
            sys.version_info = runtime_version
        assert name not in sys.modules, context
        assert 'XRootD.client._asyncio' not in sys.modules, context
        assert 'fsspec' not in sys.modules, context
        assert client.File() is not None, context
        assert client.FileSystem('root://localhost') is not None, context
        assert callable(client.open), context

# Test the actual runtime last, because successful imports remain cached.
for name in ('aio', 'asyncstream'):
    if sys.version_info < (3, 11):
        try:
            importlib.import_module('XRootD.client.' + name)
        except ImportError as error:
            assert 'require Python >= 3.11' in str(error)
        else:
            raise AssertionError('unsupported feature was imported')
    else:
        importlib.import_module('XRootD.client.' + name)
assert client.File() is not None
''', json.dumps([MODULES, OLDER_VERSIONS]))


@pytest.mark.parametrize('version', OLDER_VERSIONS)
def test_version_guard_rejects_older_python(monkeypatch, version):
    # Exercise the guard in-process to cover its rejection branch.
    source = Path(client.__file__).with_name('_asyncio.py')
    monkeypatch.setattr(sys, 'version_info', version)
    with pytest.raises(ImportError, match=r'require Python >= 3\.11'):
        runpy.run_path(str(source))


@pytest.mark.parametrize('version', [(3, 11), (3, 14)])
def test_version_guard_accepts_supported_python(monkeypatch, version):
    source = Path(client.__file__).with_name('_asyncio.py')
    monkeypatch.setattr(sys, 'version_info', version)
    namespace = runpy.run_path(str(source))
    assert namespace['asyncio'] is asyncio


def test_packaged_sources_compile_on_actual_interpreter():
    # On Alma 8 this catches modern syntax even in modules we never import.
    package = Path(client.__file__).parent.parent
    for source in package.rglob('*.py'):
        compile(source.read_text(), str(source), 'exec')

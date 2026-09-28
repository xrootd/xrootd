"""Classic bindings remain usable when optional modern APIs are rejected."""

import asyncio
import os
from pathlib import Path
import runpy
import subprocess
import sys

import pytest

from XRootD import client


MODULES = ['aio', 'asyncstream']
OLDER_VERSIONS = [(3, 6), (3, 8), (3, 9), (3, 10)]


def run_python(source, *args):
    # CTest runs against the installed wheel; propagate that same import path
    # to the fresh interpreter instead of accidentally testing another install.
    environ = dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path))
    result = subprocess.run([sys.executable, '-c', source] + list(args),
                            env=environ, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, universal_newlines=True,
                            timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr


def test_classic_import_does_not_load_modern_interfaces():
    run_python('''
import sys
from XRootD import client
for name in ('_asyncio', 'aio', 'asyncstream', 'fsspec'):
    assert 'XRootD.client.' + name not in sys.modules
assert callable(client.open)
assert client.File() is not None
assert client.FileSystem('root://localhost') is not None
assert 'fsspec' not in sys.modules
''')


@pytest.mark.parametrize('module', MODULES)
@pytest.mark.parametrize('version', OLDER_VERSIONS)
def test_unsupported_import_preserves_classic_bindings(module, version):
    run_python('''
import importlib
import sys
from XRootD import client
version = sys.version_info
sys.version_info = tuple(int(part) for part in sys.argv[2].split('.'))
try:
    try:
        importlib.import_module('XRootD.client.' + sys.argv[1])
    except ImportError as error:
        assert 'require Python >= 3.11' in str(error)
        assert 'classic XRootD.client bindings' in str(error)
    else:
        raise AssertionError('unsupported feature was imported')
finally:
    sys.version_info = version
assert 'fsspec' not in sys.modules
assert client.File() is not None
assert client.FileSystem('root://localhost') is not None
assert callable(client.open)
''', module, '.'.join(str(part) for part in version))


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


def test_actual_runtime_policy():
    run_python('''
import importlib
import sys
from XRootD import client
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
''')


def test_packaged_sources_compile_on_actual_interpreter():
    # On Alma 8 this catches modern syntax even in modules we never import.
    package = Path(client.__file__).parent.parent
    for source in package.rglob('*.py'):
        compile(source.read_text(), str(source), 'exec')

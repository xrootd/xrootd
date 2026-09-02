import pytest
import os
import glob as norm_glob
import shutil
import tempfile
import XRootD.client.glob_funcs as glob
from XRootD import client
from XRootD.client.flags import OpenFlags
from pathlib import Path
from env import *


@pytest.fixture(scope='module')
def remote_tree():
    """Create a small directory tree on the test server to glob over.

    Parent directories are created on demand when a file is opened for
    writing, and the whole tree is removed together with the server area
    when the server fixture is torn down.
    """
    for path in ('dataset1/run_1.dat', 'dataset1/run_2.dat',
                 'dataset1/run_10.dat', 'dataset2/run_3.dat'):
        f = client.File()
        status, __ = f.open(SERVER_URL + '/tmp/glob/' + path, OpenFlags.DELETE)
        assert status.ok
        status, __ = f.write('data')
        assert status.ok
        status, __ = f.close()
        assert status.ok

    return SERVER_URL + '/tmp/glob'


@pytest.fixture
def tmptree():
    # Use a short-lived directory instead of the pytest tmpdir, which is
    # retained for a few runs, so that nothing is left behind under /tmp.
    tmpdir = Path(tempfile.mkdtemp(prefix='pyxrootd-glob-', dir='/tmp'))
    subdir1 = tmpdir / "subdir1"
    subdir1.mkdir()
    subdir2 = tmpdir / "subdir2"
    subdir2.mkdir()
    for i in range(3):
        dummy = subdir1 / ("a_file_%d.txt" % i)
        dummy.write_text("This is file %d\n" % i, encoding="utf-8")
    yield tmpdir
    shutil.rmtree(tmpdir, ignore_errors=True)


def test_glob_local(tmptree):
    normal_glob_result = norm_glob.glob(str(tmptree / "not-there"))
    assert glob.glob(str(tmptree / "not-there"), raise_error=False) == normal_glob_result
    assert len(glob.glob(str(tmptree / "not-there"), raise_error=False)) == 0
    assert len(glob.glob(str(tmptree / "not-there*"), raise_error=False)) == 0
    assert len(glob.glob(str(tmptree / "sub*"), raise_error=False)) == 2
    assert len(glob.glob(str(tmptree / "subdir1" / "*txt"), raise_error=False)) == 3
    assert len(glob.glob(str(tmptree / "subdir*" / "*txt"), raise_error=False)) == 3

    with pytest.raises(RuntimeError) as excinfo:
        glob.glob(str(tmptree / "not-there"), raise_error=True)
    assert "[ERROR]" in str(excinfo.value)
    assert str(tmptree) in str(excinfo.value)


def test_glob_remote(remote_tree):
    assert len(glob.glob(remote_tree + "/nonexistent/")) == 0
    assert len(glob.glob(remote_tree + "/dataset*")) == 2
    assert len(glob.glob(remote_tree + "/dataset1/*")) == 3
    assert len(glob.glob(remote_tree + "/dataset*/*.dat")) == 4

    with pytest.raises(RuntimeError) as excinfo:
        glob.glob(remote_tree + "/nonexistent/*", raise_error=True)
    assert "[ERROR]" in str(excinfo.value)


def test_extract_url_params():
    """Test URL parameter extraction from pathnames"""

    # Test with URL parameters
    path, params = glob.extract_url_params("root://server//path/*.txt?authz=TOKEN&foo=bar")
    assert path == "root://server//path/*.txt"
    assert params == "?authz=TOKEN&foo=bar"

    # Test without URL parameters
    path, params = glob.extract_url_params("root://server//path/*.txt")
    assert path == "root://server//path/*.txt"
    assert params == ""

    # Test with glob wildcard '?' (should not be treated as URL param)
    path, params = glob.extract_url_params("/path/file?.txt")
    assert path == "/path/file?.txt"
    assert params == ""

    # Test complex case with both glob wildcard and URL params
    path, params = glob.extract_url_params("root://server//path/file?.txt?key=value")
    assert path == "root://server//path/file?.txt"
    assert params == "?key=value"

    # Test multiple URL parameters
    path, params = glob.extract_url_params("root://server//path/*?a=1&b=2&c=3")
    assert path == "root://server//path/*"
    assert params == "?a=1&b=2&c=3"

    # Test with authentication token
    path, params = glob.extract_url_params("root://server//path/*.root?authz=Bearer_TOKEN123")
    assert path == "root://server//path/*.root"
    assert params == "?authz=Bearer_TOKEN123"


def test_glob_local_with_url_params(tmptree):
    """Test that URL parameters are preserved in local glob results"""

    # Test with single file pattern and simple parameter
    results = glob.glob(str(tmptree / "subdir1" / "*txt") + "?param=value")
    assert len(results) == 3
    for result in results:
        assert result.endswith("?param=value")

    # Test with directory wildcard and multiple parameters
    results = glob.glob(str(tmptree / "subdir*" / "*txt") + "?foo=bar&baz=qux")
    assert len(results) == 3
    for result in results:
        assert result.endswith("?foo=bar&baz=qux")

    # Test with authentication-like token parameter
    results = glob.glob(str(tmptree / "subdir1" / "a_file_*.txt") + "?authz=TOKEN123")
    assert len(results) == 3
    for result in results:
        assert "?authz=TOKEN123" in result


def test_iglob_with_url_params(tmptree):
    """Test that iglob preserves URL parameters"""

    # Test iglob with URL parameters
    results = list(glob.iglob(str(tmptree / "subdir1" / "*txt") + "?test=123"))
    assert len(results) == 3
    for result in results:
        assert "?test=123" in result

    # Test iglob with multiple parameters
    results = list(glob.iglob(str(tmptree / "subdir*" / "*txt") + "?key1=val1&key2=val2"))
    assert len(results) == 3
    for result in results:
        assert "?key1=val1&key2=val2" in result


def test_glob_remote_with_url_params(remote_tree):
    """Test that URL parameters work with remote XRootD paths"""

    url_params = "?xrd.wantprot=unix"

    results = glob.glob(remote_tree + "/dataset1/*" + url_params)

    # Should get results
    assert len(results) == 3

    # All results should have URL parameters preserved
    for result in results:
        assert result.endswith(url_params)


def test_multiple_glob_with_url_params(remote_tree):
    """Test multiple glob patterns with URL parameters"""
    results = glob.glob(remote_tree + "/dataset*/*.dat?xrd.wantprot=unix")

    # Ensure we have results from all subdirectories
    assert len(results) == 4

    # Check that URL parameters are preserved in all results
    for result in results:
        assert "?xrd.wantprot=unix" in result


def test_folder_glob_with_url_params(remote_tree):
    """Test globbing across folders with URL parameters"""

    # The single character wildcard must match run_1 to run_3, but not run_10,
    # and must not be confused with the start of the URL parameters.

    results = glob.glob(remote_tree + "/dataset*/run_?.dat?xrd.wantprot=unix")
    expected = [
        "dataset1/run_1.dat?xrd.wantprot=unix",
        "dataset1/run_2.dat?xrd.wantprot=unix",
        "dataset2/run_3.dat?xrd.wantprot=unix",
    ]
    assert sorted(results) == [remote_tree + "/" + path for path in expected]


def test_glob_backward_compatibility(tmptree):
    """Ensure existing glob functionality still works without URL parameters"""

    # All existing tests should still pass
    assert len(glob.glob(str(tmptree / "sub*"))) == 2
    assert len(glob.glob(str(tmptree / "subdir1" / "*txt"))) == 3
    assert len(glob.glob(str(tmptree / "subdir*" / "*txt"))) == 3

    # Test that paths without parameters are unchanged
    results = glob.glob(str(tmptree / "subdir1" / "*txt"))
    for result in results:
        assert "?" not in result

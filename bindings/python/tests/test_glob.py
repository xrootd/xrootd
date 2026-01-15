import pytest
import os
import glob as norm_glob
import XRootD.client.glob_funcs as glob
from pathlib import Path

LHCB_EOS_PUBLIC_PREFIX = (
    "root://eospublic.cern.ch//eos/opendata/lhcb/Collision11/CHARM/"
    "LHCb_2011_Beam3500GeV_VeloClosed_MagDown_RealData_Reco14_Stripping21r1_CHARM_MDST/"
)


@pytest.fixture
def tmptree(tmpdir):
    subdir1 = tmpdir / "subdir1"
    subdir1.mkdir()
    subdir2 = tmpdir / "subdir2"
    subdir2.mkdir()
    for i in range(3):
        dummy = subdir1 / ("a_file_%d.txt" % i)
        dummy.write_text("This is file %d\n" % i, encoding="utf-8")
    return tmpdir


def test_glob_local(tmptree):
    normal_glob_result = norm_glob.glob(str(tmptree / "not-there"))
    assert glob.glob(str(tmptree / "not-there")) == normal_glob_result
    assert len(glob.glob(str(tmptree / "not-there"))) == 0
    assert len(glob.glob(str(tmptree / "not-there*"))) == 0
    assert len(glob.glob(str(tmptree / "sub*"))) == 2
    assert len(glob.glob(str(tmptree / "subdir1" / "*txt"))) == 3
    assert len(glob.glob(str(tmptree / "subdir*" / "*txt"))) == 3

    with pytest.raises(RuntimeError) as excinfo:
        glob.glob(str(tmptree / "not-there"), raise_error=True)
    assert "[ERROR]" in str(excinfo.value)
    assert str(tmptree) in str(excinfo.value)


def test_glob_remote(tmptree):
    assert len(glob.glob("root://eospublic.cern.ch//eos/root-eos/cms_opendata_2012_nanoad/")) == 0
    assert len(glob.glob("root://eospublic.cern.ch//eos/root-eos/cms_opendata_2012_nanoa*")) == 2
    assert len(glob.glob("root://eospublic.cern.ch//eos/root-eos/cms_opendata_2012_nanoaod/*")) > 0
    assert len(glob.glob("root://eospublic.cern.ch//eos/root-*/cms_opendata_2012_nanoaod/*")) > 0

    with pytest.raises(RuntimeError) as excinfo:
        glob.glob("root://eospublic.cern.ch//eos/root-NOTREAL/cms_opendata_2012_nanoaod/*", raise_error=True)
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


def test_glob_remote_with_url_params():
    """Test that URL parameters work with remote XRootD paths"""

    # Test with public endpoint and URL parameters
    base_path = "root://eospublic.cern.ch//eos/root-eos/cms_opendata_2012_nanoaod/*"
    url_params = "?xrd.wantprot=unix"

    results = glob.glob(base_path + url_params)

    # Should get results
    assert len(results) > 0

    # All results should have URL parameters preserved
    for result in results:
        assert url_params in result


def test_multiple_glob_with_url_params():
    """Test multiple glob patterns with URL parameters"""
    results = glob.glob(LHCB_EOS_PUBLIC_PREFIX + "00041840/*/*.mdst?xrd.wantprot=unix")

    # Ensure we have results from all subdirectories
    assert len(results) > 4000

    # Check that URL parameters are preserved in all results
    for result in results:
        assert "?xrd.wantprot=unix" in result


def test_folder_glob_with_url_params(tmptree):
    """Test globbing for folders with URL parameters"""
    pattern = (
        LHCB_EOS_PUBLIC_PREFIX + "00041840/*/00041840_000?4866_1.charm.mdst?xrd.wantprot=unix"
    )
    results = glob.glob(pattern)
    expected = [
        "00041840/0001/00041840_00014866_1.charm.mdst?xrd.wantprot=unix",
        "00041840/0003/00041840_00034866_1.charm.mdst?xrd.wantprot=unix",
        "00041840/0004/00041840_00044866_1.charm.mdst?xrd.wantprot=unix",
        "00041840/0006/00041840_00064866_1.charm.mdst?xrd.wantprot=unix",
    ]
    assert results == [LHCB_EOS_PUBLIC_PREFIX + path for path in expected]


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

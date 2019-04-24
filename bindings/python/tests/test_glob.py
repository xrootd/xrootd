import pytest
import glob as norm_glob
import XRootD.client.glob_funcs as glob
from pathlib2 import Path


@pytest.fixture
def tmptree(tmpdir):
    subdir1 = tmpdir / "subdir1"
    subdir1.mkdir()
    subdir2 = tmpdir / "subdir2"
    subdir2.mkdir()
    for i in range(3):
        dummy = subdir1 / ("a_file_%d.txt" % i)
        dummy.write_text(u"This is file %d\n" % i, encoding="utf-8")
    return tmpdir


def test_local_glob(tmptree):
    assert len(glob.glob(str(tmptree / "not-there"))) == 0
    assert len(glob.glob(str(tmptree / "not-there*"))) == 0
    assert len(glob.glob(str(tmptree / "sub*"))) == 2
    assert len(glob.glob(str(tmptree / "subdir1" / "*txt"))) == 3
    assert len(glob.glob(str(tmptree / "subdir*" / "*txt"))) == 3


def test_xrootd_glob(tmptree):
    tmptree = Path("root://localhost:") / tmptree
    assert glob.glob(str(tmptree / "not-there")) == norm_glob.glob(str(tmptree / "not-there"))
    assert len(glob.glob(str(tmptree / "not-there"))) == 0
    assert len(glob.glob(str(tmptree / "not-there*"))) == 0
    assert len(glob.glob(str(tmptree / "sub*"))) == 2
    assert len(glob.glob(str(tmptree / "subdir1" / "*txt"))) == 3
    assert len(glob.glob(str(tmptree / "subdir*" / "*txt"))) == 3

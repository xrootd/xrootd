import pytest
import os
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


@pytest.mark.parametrize("prefix", ["", r"root://localhost:"])
def test_glob(tmptree, prefix):
    tmptree = str(tmptree)
    if prefix:
        tmptree = prefix + tmptree
    normal_glob_result = norm_glob.glob(os.path.join(tmptree, "not-there"))
    assert glob.glob(os.path.join(tmptree, "not-there")) == normal_glob_result
    assert len(glob.glob(os.path.join(tmptree, "not-there"))) == 0
    assert len(glob.glob(os.path.join(tmptree, "not-there*"))) == 0
    assert len(glob.glob(os.path.join(tmptree, "sub*"))) == 2
    assert len(glob.glob(os.path.join(tmptree, "subdir1", "*txt"))) == 3
    assert len(glob.glob(os.path.join(tmptree, "subdir*", "*txt"))) == 3

    with pytest.raises(RuntimeError) as excinfo:
        glob.glob(os.path.join(tmptree, "not-there"), raise_error=True)
    assert "[ERROR]" in str(excinfo.value)
    assert str(tmptree) in str(excinfo.value)

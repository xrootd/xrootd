from XRootD.client import fsutils
from XRootD.client.flags import MkDirFlags, StatInfoFlags


class Status(object):
    def __init__(self, ok=True):
        self.ok = ok
        self.message = "OK" if ok else "error"


class StatInfo(object):
    def __init__(self, size=0, flags=0):
        self.size = size
        self.flags = flags


class Entry(object):
    def __init__(self, name, statinfo):
        self.name = name
        self.hostaddr = None
        self.statinfo = statinfo


class FakeFileSystem(object):
    def __init__(self):
        self.removed_files = []
        self.removed_dirs = []
        self.mkdir_calls = []
        self.tree = {
            "/data": [
                Entry("one.dat", StatInfo(size=10)),
                Entry("nested", StatInfo(flags=StatInfoFlags.IS_DIR)),
            ],
            "/data/nested": [
                Entry("two.dat", StatInfo(size=20)),
            ],
        }

    def dirlist(self, path, flags, timeout=0):
        return Status(), list(self.tree.get(path, []))

    def mkdir(self, path, flags, mode=0, timeout=0):
        self.mkdir_calls.append((path, flags, mode, timeout))
        return Status(), None

    def rm(self, path, timeout=0):
        self.removed_files.append(path)
        return Status(), None

    def rmdir(self, path, timeout=0):
        self.removed_dirs.append(path)
        return Status(), None


def test_list_tree_returns_absolute_entries():
    status, entries = fsutils.list_tree(FakeFileSystem(), "/data")

    assert status.ok
    assert [entry.path for entry in entries] == [
        "/data/one.dat",
        "/data/nested",
        "/data/nested/two.dat",
    ]


def test_directory_size_can_be_recursive():
    status, result = fsutils.directory_size(FakeFileSystem(), "/data", recursive=True)

    assert status.ok
    assert result.files == 2
    assert result.subdirs == 1
    assert result.size == 30
    assert result.as_dict() == {"Files": 2, "Size": 30, "SubDirs": 1}


def test_remove_tree_removes_files_before_directories():
    filesystem = FakeFileSystem()

    status, result = fsutils.remove_tree(filesystem, "/data")

    assert status.ok
    assert filesystem.removed_files == ["/data/one.dat", "/data/nested/two.dat"]
    assert filesystem.removed_dirs == ["/data/nested", "/data"]
    assert result.files_removed == 2
    assert result.directories_removed == 2
    assert result.size_removed == 30


def test_mkdir_p_uses_makepath_flag():
    filesystem = FakeFileSystem()

    status, _ = fsutils.mkdir_p(filesystem, "/data/new/path", mode=493, timeout=7)

    assert status.ok
    assert filesystem.mkdir_calls == [
        ("/data/new/path", MkDirFlags.MAKEPATH, 493, 7),
    ]

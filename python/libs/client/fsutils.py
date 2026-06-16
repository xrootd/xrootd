# -------------------------------------------------------------------------------
# Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
# -------------------------------------------------------------------------------
# This file is part of the XRootD software suite.
#
# XRootD is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# XRootD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
# -------------------------------------------------------------------------------
from __future__ import absolute_import, division, print_function

import posixpath

from XRootD.client.flags import DirListFlags, MkDirFlags, StatInfoFlags


class DirectoryEntry(object):
    """A normalized directory-listing entry with its absolute remote path."""

    def __init__(self, parent, entry):
        self.parent = parent
        self.name = entry.name
        self.path = remote_join(parent, entry.name)
        self.hostaddr = getattr(entry, "hostaddr", None)
        self.statinfo = entry.statinfo

    @property
    def is_directory(self):
        return is_directory(self.statinfo)

    @property
    def is_file(self):
        return is_file(self.statinfo)

    @property
    def size(self):
        return file_size(self.statinfo)


class DirectorySize(object):
    """Directory accounting information.

    :var files:      number of files
    :var size:       summed file size in bytes
    :var subdirs:    number of sub-directories
    """

    def __init__(self):
        self.files = 0
        self.size = 0
        self.subdirs = 0

    def add(self, entry):
        if entry.is_directory:
            self.subdirs += 1
        elif entry.is_file:
            self.files += 1
            self.size += entry.size

    def as_dict(self):
        return {"Files": self.files, "Size": self.size, "SubDirs": self.subdirs}


class RemoveTreeResult(object):
    """Recursive deletion accounting information."""

    def __init__(self):
        self.files_removed = 0
        self.directories_removed = 0
        self.size_removed = 0

    def as_dict(self):
        return {
            "FilesRemoved": self.files_removed,
            "DirectoriesRemoved": self.directories_removed,
            "SizeRemoved": self.size_removed,
        }


def remote_join(parent, name):
    """Join XRootD paths without using platform-specific path separators."""
    if not parent:
        parent = "/"
    return posixpath.join(parent.rstrip("/") or "/", name)


def is_directory(statinfo):
    """Return ``True`` when a stat response describes a directory."""
    return bool(statinfo and statinfo.flags & StatInfoFlags.IS_DIR)


def is_file(statinfo):
    """Return ``True`` when a stat response describes a regular file."""
    if not statinfo:
        return False
    return not bool(statinfo.flags & (StatInfoFlags.IS_DIR | StatInfoFlags.OTHER))


def file_size(statinfo):
    """Return a stat response size, or ``0`` when no size is available."""
    if not statinfo:
        return 0
    return int(getattr(statinfo, "size", 0) or 0)


def mkdir_p(filesystem, path, mode=0, timeout=0):
    """Create a directory and its parents.

    This is a small GFAL-style convenience wrapper around
    :meth:`XRootD.client.FileSystem.mkdir` using ``MkDirFlags.MAKEPATH``.
    """
    return filesystem.mkdir(path, MkDirFlags.MAKEPATH, mode, timeout)


def list_directory(filesystem, path, timeout=0):
    """List one directory with stat information.

    :returns: tuple containing ``XRootDStatus`` and a list of
              :class:`DirectoryEntry` objects.
    """
    status, directory = filesystem.dirlist(path, DirListFlags.STAT, timeout)
    if not status.ok:
        return status, []
    return status, [DirectoryEntry(path, entry) for entry in directory]


def list_tree(filesystem, path, timeout=0):
    """Recursively list a directory tree.

    The returned list is depth-first and contains entries below ``path``; the
    root path itself is not included.
    """
    status, entries = list_directory(filesystem, path, timeout)
    if not status.ok:
        return status, []

    tree = []
    for entry in entries:
        tree.append(entry)
        if entry.is_directory:
            status, children = list_tree(filesystem, entry.path, timeout)
            tree.extend(children)
            if not status.ok:
                return status, tree
    return status, tree


def directory_size(filesystem, path, recursive=False, timeout=0):
    """Return file, byte, and sub-directory counts for a directory."""
    if recursive:
        status, entries = list_tree(filesystem, path, timeout)
    else:
        status, entries = list_directory(filesystem, path, timeout)

    result = DirectorySize()
    for entry in entries:
        result.add(entry)
    return status, result


def remove_tree(filesystem, path, timeout=0):
    """Recursively remove a directory tree.

    Files are removed before directories. The returned result contains counts for
    successfully removed files/directories and the summed size of removed files.
    """
    result = RemoveTreeResult()
    status, entries = list_directory(filesystem, path, timeout)
    if not status.ok:
        return status, result

    for entry in entries:
        if entry.is_directory:
            status, child_result = remove_tree(filesystem, entry.path, timeout)
            result.files_removed += child_result.files_removed
            result.directories_removed += child_result.directories_removed
            result.size_removed += child_result.size_removed
            if not status.ok:
                return status, result
        elif entry.is_file:
            status, _ = filesystem.rm(entry.path, timeout)
            if not status.ok:
                return status, result
            result.files_removed += 1
            result.size_removed += entry.size

    status, _ = filesystem.rmdir(path, timeout)
    if status.ok:
        result.directories_removed += 1
    return status, result

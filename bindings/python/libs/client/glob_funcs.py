#-------------------------------------------------------------------------------
# Copyright (c) 2012-2018 by European Organization for Nuclear Research (CERN)
# Author: Benjamin Krikler <b.krikler@cern.ch>
#-------------------------------------------------------------------------------
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
#
# In applying this licence, CERN does not waive the privileges and immunities
# granted to it by virtue of its status as an Intergovernmental Organization
# or submit itself to any jurisdiction.
#-------------------------------------------------------------------------------
from __future__ import absolute_import, division, print_function

from XRootD.client.filesystem import FileSystem

import glob as gl
import os
import fnmatch
import sys
if sys.version_info[0] > 2:
    from urllib.parse import urlparse
else:
    from urlparse import urlparse


__all__ = ["glob", "iglob"]


def split_url(url):
    parsed_uri = urlparse(url)
    domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_uri)
    path = parsed_uri.path
    return domain, path


def glob(pathname):
    # Let normal python glob try first
    try_glob = gl.glob(pathname)
    if try_glob:
        return try_glob

    # If pathname does not contain a wildcard:
    if not gl.has_magic(pathname):
        return [pathname]

    # Else try xrootd instead
    return xrootd_glob(pathname)


def xrootd_glob(pathname):
    # Split the pathname into a directory and basename
    dirs, basename = os.path.split(pathname)

    if gl.has_magic(dirs):
        dirs = xrootd_glob(dirs)
    else:
        dirs = [dirs]

    files = []
    for dirname in dirs:
        host, path = split_url(dirname)
        query = FileSystem(host)

        if not query:
            raise RuntimeError("Cannot prepare xrootd query")

        _, dirlist = query.dirlist(path)
        for entry in dirlist.dirlist:
            filename = entry.name
            if filename in [".", ".."]:
                continue
            if not fnmatch.fnmatchcase(filename, basename):
                continue
            files.append(os.path.join(dirname, filename))

    return files


def iglob(pathname):
    for name in glob(pathname):
        yield name

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


def iglob(pathname, raise_error=False):
    """
    Generates paths based on a wild-carded path, potentially via xrootd.

    Multiple wild-cards can be present in the path.

    Args:
      pathname (str):  The wild-carded path to be expanded.
      raise_error (bool):  Whether or not to let xrootd raise an error if
          there's a problem.  If False (default), and there's a problem for a
          particular directory or file, then that will simply be skipped,
          likely resulting in an empty list.

    Yields:
      (str): A single path that matches the wild-carded string
    """
    # Let normal python glob try first
    generator = gl.iglob(pathname)
    path = next(generator, None)
    if path is not None:
        yield path
        for path in generator:
            yield path
        return

    # Else try xrootd instead
    for path in xrootd_iglob(pathname, raise_error=raise_error):
        yield path


def xrootd_iglob(pathname, raise_error):
    """Handles the actual interaction with xrootd

    Provides a python generator over files that match the wild-card expression.
    """
    # Split the pathname into a directory and basename
    dirs, basename = os.path.split(pathname)

    if gl.has_magic(dirs):
        dirs = list(xrootd_iglob(dirs, raise_error))
    else:
        dirs = [dirs]

    for dirname in dirs:
        host, path = split_url(dirname)
        query = FileSystem(host)

        if not query:
            raise RuntimeError("Cannot prepare xrootd query")

        status, dirlist = query.dirlist(path)
        if status.error:
            if not raise_error:
                continue
            raise RuntimeError("'{!s}' for path '{}'".format(status, dirname))

        for entry in dirlist.dirlist:
            filename = entry.name
            if filename in [".", ".."]:
                continue
            if not fnmatch.fnmatchcase(filename, basename):
                continue
            yield os.path.join(dirname, filename)


def glob(pathname, raise_error=False):
    """
    Creates a list of paths that match pathname.

    Multiple wild-cards can be present in the path.

    Args:
      pathname (str):  The wild-carded path to be expanded.
      raise_error (bool):  Whether or not to let xrootd raise an error if
          there's a problem.  If False (default), and there's a problem for a
          particular directory or file, then that will simply be skipped,
          likely resulting in an empty list.

    Returns:
      (str): A single path that matches the wild-carded string
    """
    return list(iglob(pathname, raise_error=raise_error))

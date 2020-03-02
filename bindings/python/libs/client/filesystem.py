#-------------------------------------------------------------------------------
# Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
# Author: Justin Salmon <jsalmon@cern.ch>
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

from pyxrootd import client
from XRootD.client.responses import XRootDStatus, StatInfo, StatInfoVFS
from XRootD.client.responses import LocationInfo, DirectoryList, ProtocolInfo
from XRootD.client.utils import CallbackWrapper
from XRootD.client.flags import AccessMode

class FileSystem(object):
  """Interact with an ``xrootd`` server to perform filesystem-based operations
  such as copying files, creating directories, changing file permissions,
  listing directories, etc.

  :param url: The URL of the server to connect with
  :type  url: string
  """

  def __init__(self, url):
    self.__fs = client.FileSystem(url)

  @property
  def url(self):
    """The server URL object, instance of :mod:`XRootD.client.URL`"""
    return self.__fs.url

  def copy(self, source, target, force=False):
    """Copy a file.

    .. note:: This method is less configurable than using
              :mod:`XRootD.client.CopyProcess` - it is designed to be as simple
              as possible by using sensible defaults for the underlying copy
              job. If you need more configurability, or want to make multiple
              copy jobs run at once in parallel, use
              :mod:`XRootD.client.CopyProcess`.

    :param source: Source file path
    :type  source: string
    :param target: Destination file path
    :type  target: string
    :param  force: overwrite target if it exists
    :type   force: boolean
    :returns:      tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                   object and None
    """
    result = self.__fs.copy(source=source, target=target, force=force)[0]
    return XRootDStatus(result), None

  def locate(self, path, flags, timeout=0, callback=None):
    """Locate a file.

    :param  path: path to the file to be located
    :type   path: string
    :param flags: An `ORed` combination of :mod:`XRootD.client.flags.OpenFlags`
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`XRootD.client.responses.LocationInfo` object
    """
    if callback:
      callback = CallbackWrapper(callback, LocationInfo)
      return XRootDStatus(self.__fs.locate(path, flags, timeout, callback))

    status, response = self.__fs.locate(path, flags, timeout)
    if response: response = LocationInfo(response)
    return XRootDStatus(status), response

  def deeplocate(self, path, flags, timeout=0, callback=None):
    """Locate a file, recursively locate all disk servers.

    :param  path: path to the file to be located
    :type   path: string
    :param flags: An `ORed` combination of :mod:`XRootD.client.flags.OpenFlags`
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`XRootD.client.responses.LocationInfo` object
    """
    if callback:
      callback = CallbackWrapper(callback, LocationInfo)
      return XRootDStatus(self.__fs.deeplocate(path, flags, timeout, callback))

    status, response = self.__fs.deeplocate(path, flags, timeout)
    if response: response = LocationInfo(response)
    return XRootDStatus(status), response

  def mv(self, source, dest, timeout=0, callback=None):
    """Move a directory or a file.

    :param source: the file or directory to be moved
    :type  source: string
    :param   dest: the new name
    :type    dest: string
    :returns:      tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                   object and None
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.mv(source, dest, timeout, callback))

    status, response = self.__fs.mv(source, dest, timeout)
    return XRootDStatus(status), None

  def query(self, querycode, arg, timeout=0, callback=None):
    """Obtain server information.

    :param querycode: the query code as specified in
                      :mod:`XRootD.client.flags.QueryCode`
    :param       arg: query argument
    :type        arg: string
    :returns:         the query response or None if there was an error
    :rtype:           string

    .. note::
      For more information about XRootD query codes and arguments, see
      `the relevant section in the protocol reference
      <http://xrootd.slac.stanford.edu/doc/prod/XRdv299.htm#_Toc337053385>`_.
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.query(querycode, arg, timeout, callback))

    status, response = self.__fs.query(querycode, arg, timeout)
    return XRootDStatus(status), response

  def truncate(self, path, size, timeout=0, callback=None):
    """Truncate a file.

    :param path: path to the file to be truncated
    :type  path: string
    :param size: file size
    :type  size: integer
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and None
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.truncate(path, size, timeout, callback))

    status, response = self.__fs.truncate(path, size, timeout)
    return XRootDStatus(status), None

  def rm(self, path, timeout=0, callback=None):
    """Remove a file.

    :param path: path to the file to be removed
    :type  path: string
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and None
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.rm(path, timeout, callback))

    status, response = self.__fs.rm(path, timeout)
    return XRootDStatus(status), None

  def mkdir(self, path, flags=0, mode=0, timeout=0, callback=None):
    """Create a directory.

    :param  path: path to the directory to create
    :type   path: string
    :param flags: An `ORed` combination of :mod:`XRootD.client.flags.MkDirFlags`
                  where the default is `MkDirFlags.NONE`
    :param  mode: the initial file access mode, an `ORed` combination of
                  :mod:`XRootD.client.flags.AccessMode` where the default is
                  `rwxr-x---`
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and None
    """
    if mode == 0:
      mode = AccessMode.UR | AccessMode.UW | AccessMode.UX | \
             AccessMode.GR | AccessMode.GX

    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.mkdir(path, flags, mode, timeout, callback))

    status, response = self.__fs.mkdir(path, flags, mode, timeout)
    return XRootDStatus(status), None

  def rmdir(self, path, timeout=0, callback=None):
    """Remove a directory.

    :param path: path to the directory to remove
    :type  path: string
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and None
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.rmdir(path, timeout, callback))

    status, response = self.__fs.rmdir(path, timeout)
    return XRootDStatus(status), None

  def chmod(self, path, mode, timeout=0, callback=None):
    """Change access mode on a directory or a file.

    :param path: path to the file/directory to change access mode
    :type  path: string
    :param mode: An `OR`ed` combination of :mod:`XRootD.client.flags.AccessMode`
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and None
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.chmod(path, mode, timeout, callback))

    status, response = self.__fs.chmod(path, mode, timeout)
    return XRootDStatus(status), None

  def ping(self, timeout=0, callback=None):
    """Check if the server is alive.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              object and None
    """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.ping(timeout, callback))

    status, response = self.__fs.ping(timeout)
    return XRootDStatus(status), None

  def stat(self, path, timeout=0, callback=None):
    """Obtain status information for a path.

    :param path: path to the file/directory to stat
    :type  path: string
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and :mod:`XRootD.client.responses.StatInfo` object
    """
    if callback:
      callback = CallbackWrapper(callback, StatInfo)
      return XRootDStatus(self.__fs.stat(path, timeout, callback))

    status, response = self.__fs.stat(path, timeout)
    if response: response = StatInfo(response)
    return XRootDStatus(status), response

  def statvfs(self, path, timeout=0, callback=None):
    """Obtain status information for a Virtual File System.

    :param path: path to the file/directory to stat
    :type  path: string
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and :mod:`XRootD.client.responses.StatInfoVFS` object
    """
    if callback:
      callback = CallbackWrapper(callback, StatInfoVFS)
      return XRootDStatus(self.__fs.statvfs(path, timeout, callback))

    status, response = self.__fs.statvfs(path, timeout)
    if response: response = StatInfoVFS(response)
    return XRootDStatus(status), response

  def protocol(self, timeout=0, callback=None):
    """Obtain server protocol information.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              object and :mod:`XRootD.client.responses.ProtocolInfo` object
    """
    if callback:
      callback = CallbackWrapper(callback, ProtocolInfo)
      return XRootDStatus(self.__fs.protocol(timeout, callback))

    status, response = self.__fs.protocol(timeout)
    if response: response = ProtocolInfo(response)
    return XRootDStatus(status), response

  def dirlist(self, path, flags=0, timeout=0, callback=None):
    """List entries of a directory.

    :param  path: path to the directory to list
    :type   path: string
    :param flags: An `ORed` combination of :mod:`XRootD.client.flags.DirListFlags`
                  where the default is `DirListFlags.NONE`
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`XRootD.client.responses.DirectoryList` object

    .. warning:: Currently, passing `DirListFlags.STAT` with an asynchronous
                 call to :mod:`XRootD.client.FileSystem.dirlist()` does not
                 work, due to an xrootd client limitation. So you'll get
                 ``None`` instead of the ``StatInfo`` instance. See
                 `the GitHub issue <https://github.com/xrootd/xrootd/issues/2>`_
                 for more details.
    """
    if callback:
      callback = CallbackWrapper(callback, DirectoryList)
      return XRootDStatus(self.__fs.dirlist(path, flags, timeout, callback))

    status, response = self.__fs.dirlist(path, flags, timeout)
    if response: response = DirectoryList(response)
    return XRootDStatus(status), response

  def sendinfo(self, info, timeout=0, callback=None):
    """Send info to the server (up to 1024 characters).

    :param info: the info string to be sent
    :type  info: string
    :returns:    tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                 object and None
     """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.sendinfo(info, timeout, callback))

    status, response = self.__fs.sendinfo(info, timeout)
    return XRootDStatus(status), response

  def prepare(self, files, flags, priority=0, timeout=0, callback=None):
    """Prepare one or more files for access.

    :param    files: list of files to be prepared
    :type     files: list
    :param    flags: An `ORed` combination of
                     :mod:`XRootD.client.flags.PrepareFlags`
    :param priority: priority of the request 0 (lowest) - 3 (highest)
    :type  priority: integer
    :returns:        tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                     object and None
     """
    if callback:
      callback = CallbackWrapper(callback, None)
      return XRootDStatus(self.__fs.prepare(files, flags, priority, timeout,
                                            callback))

    status, response = self.__fs.prepare(files, flags, priority, timeout)
    return XRootDStatus(status), response

  def set_property(self, name, value):
    """Set file system property.

    :param name: name of the property to set
    :type  name: string
    :returns:    boolean denoting if property setting was successful
    :rtype:      boolean
    """
    return self.__fs.set_property(name, value)

  def get_property(self, name):
    """Get file system property.

    :param name: name of the property
    :type  name: string
    """
    return self.__fs.get_property(name)

  def cat(self, path):
    """Cat the remote file.

    :param path: path to the remote file
    :type  path: string
    """
    source = self.__fs.url.hostid + '/' + path
    return self.__fs.cat(source)

  def set_xattr(self, path, attrs, timeout=0, callback=None):
    """Set extended file attributes.
    :param path:  path to the file
    :type  path:  string
    :param attrs: extended attributes to be set on the file
    :type  attrs: list of tuples of name/value pairs
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`list of touples (name, XRootD.client.responses.XRootDStatus)` object
    """
    if callback:
      callback = CallbackWrapper(callback, list)
      return XRootDStatus(self.__fs.set_xattr(path, attrs, timeout, callback))

    status, response = self.__fs.set_xattr(path, attrs, timeout)
    return XRootDStatus(status), response

  def get_xattr(self, path, attrs, timeout=0, callback=None):
    """Get extended file attributes.
    :param path:  path to the file
    :type  path:  string
    :param attrs: extended attributes to be set on the file
    :type  attrs: list of tuples of name/value pairs
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`list of touples (name, value, XRootD.client.responses.XRootDStatus)` object
    """
    if callback:
      callback = CallbackWrapper(callback, list)
      return XRootDStatus(self.__fs.get_xattr(path, attrs, timeout, callback))

    status, response = self.__fs.get_xattr(path, attrs, timeout)
    return XRootDStatus(status), response

  def del_xattr(self, path, attrs, timeout=0, callback=None):
    """Delete extended file attributes.
    :param path:  path to the file
    :type  path:  string
    :param attrs: extended attributes to be set on the file
    :type  attrs: list of tuples of name/value pairs
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`list of touples (name, XRootD.client.responses.XRootDStatus)` object
    """
    if callback:
      callback = CallbackWrapper(callback, list)
      return XRootDStatus(self.__fs.del_xattr(path, attrs, timeout, callback))

    status, response = self.__fs.del_xattr(path, attrs, timeout)
    return XRootDStatus(status), response

  def list_xattr(self, path, timeout=0, callback=None):
    """Delete extended file attributes.
    :param path:  path to the file
    :type  path:  string
    :param attrs: extended attributes to be set on the file
    :type  attrs: list of tuples of name/value pairs
    :returns:     tuple containing :mod:`XRootD.client.responses.XRootDStatus`
                  object and :mod:`list of touples (name, value, XRootD.client.responses.XRootDStatus)` object
    """
    if callback:
      callback = CallbackWrapper(callback, list)
      return XRootDStatus(self.__fs.list_xattr(path, timeout, callback))

    status, response = self.__fs.list_xattr(path, timeout)
    return XRootDStatus(status), response

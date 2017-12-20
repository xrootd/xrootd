#-------------------------------------------------------------------------------
# Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
# Author: Justin Salmon <jsalmon@cern.ch>
#-------------------------------------------------------------------------------
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
# along with XRootD.  If not, see <http:#www.gnu.org/licenses/>.
#-------------------------------------------------------------------------------
from __future__ import absolute_import, division, print_function

from XRootD.client.url import URL

class Struct(object):
  """Convert a dict into an object by adding each dict entry to __dict__"""
  def __init__(self, entries):
      self.__dict__.update(**entries)
  def __repr__(self):
    return '<%s>' % str(', '.join('%s: %s' % (k, repr(v))
                                  for (k, v) in self.__dict__.items()))

class LocationInfo(Struct):
  """Path location information (a list of discovered file locations).

  :param locations: (List of :mod:`XRootD.client.responses.Location` objects)
                    List of discovered locations

  This object is iterable::

    >>> status, locations = filesystem.locate('/tmp', OpenFlags.REFRESH)
    >>> print locations
    <XRootD.client.responses.LocationInfo object at 0x288b9f0>
    >>> for location in locations:
    ...   print location.address
    ...
    [::127.0.0.1]:1094

  """
  def __init__(self, locations):
    super(LocationInfo, self).__init__({'locations':
                                        [Location(l) for l in locations]})

  def __iter__(self):
    return iter(self.locations)

class Location(Struct):
  """Information about a single location.

  :var    address: The address of this location
  :var       type: The type of this location, one of
                   :mod:`XRootD.client.flags.LocationType`
  :var accesstype: The allowed access type of this location, one of
                   :mod:`XRootD.client.flags.AccessType`
  :var is_manager: Is the location a manager
  :var  is_server: Is the location a server
  """
  def __init__(self, location):
    super(Location, self).__init__(location)

class XRootDStatus(Struct):
  """Status of a request. Returned with all requests.

  :var   message: Message describing the status of this request
  :var        ok: The request was successful
  :var     error: Error making request
  :var     fatal: Fatal error making request
  :var    status: Status of the request
  :var      code: Error type, or additional hints on what to do
  :var shellcode: Status code that may be returned to the shell
  :var     errno: Errno, if any
  """
  def __init__(self, status):
    super(XRootDStatus, self).__init__(status)

  def __str__(self):
    return self.message

class ProtocolInfo(Struct):
  """Protocol information for a server.

  :var  version: The version of the protocol this server is speaking
  :var hostinfo: Informational flags for this host. An `ORed` combination of
                 :mod:`XRootD.client.flags.HostTypes`
  """
  def __init__(self, info):
    super(ProtocolInfo, self).__init__(info)

class StatInfo(Struct):
  """Status information for files and directories.

  :var         id: This file's unique identifier
  :var      flags: Informational flags. An `ORed` combination of
                   :mod:`XRootD.client.flags.StatInfoFlags`
  :var       size: The file size (in bytes)
  :var    modtime: Modification time (in seconds since epoch)
  :var modtimestr: Modification time (as readable string)
  """
  def __init__(self, info):
    super(StatInfo, self).__init__(info)

class StatInfoVFS(Struct):
  """Status information for Virtual File Systems.

  :var            nodes_rw: Number of nodes that can provide read/write space
  :var             free_rw: Size of the largest contiguous area of free r/w
                            space (in MB)
  :var      utilization_rw: Percentage of the partition utilization represented
                            by ``free_rw``
  :var       nodes_staging: Number of nodes that can provide staging space
  :var        free_staging: Size of the largest contiguous area of free staging
                            space (in MB)
  :var utilization_staging: Percentage of the partition utilization represented
                            by ``free_staging``
  """
  def __init__(self, info):
    super(StatInfoVFS, self).__init__(info)

class DirectoryList(Struct):
  """Directory listing.

  This object is iterable::

    >>> status, dirlist = filesystem.dirlist('/tmp', DirListFlags.STAT)
    >>> print dirlist
    <XRootD.client.responses.DirectoryList object at 0x288b9f0>
    >>> print 'Entries:', dirlist.size
    Entries: 2
    >>> for item in dirlist:
    ...   print item.name, item.statinfo.size
    ...
    spam 1024
    eggs 2048

  :var     size: The size of this listing (number of entries)
  :var   parent: The name of the parent directory of this directory
  :var  dirlist: (List of :mod:`XRootD.client.responses.ListEntry` objects) -
                 The list of directory entries
  """
  def __init__(self, dirlist):
    dirlist.update({'dirlist': [ListEntry(e) for e in dirlist['dirlist']]})
    super(DirectoryList, self).__init__(dirlist)

  def __iter__(self):
    return iter(self.dirlist)

class ListEntry(Struct):
  """An entry in a directory listing.

  :var      name: The name of the file/directory
  :var  hostaddr: The address of the host on which this file/directory lives
  :var  statinfo: (Instance of :mod:`XRootD.client.responses.StatInfo`) -
                  Status information about this file/directory. You must pass
                  `DirListFlags.STAT` with the call to
                  :mod:`XRootD.client.FileSystem.dirlist()` to retrieve status
                  information.
  """
  def __init__(self, entry):
    if entry['statinfo']: entry.update({'statinfo': StatInfo(entry['statinfo'])})
    super(ListEntry, self).__init__(entry)

class ChunkInfo(Struct):
  """Describes a data chunk for a vector read.

  :var offset: The offset in the file from which this chunk came
  :var length: The length of this chunk
  :var buffer: The actual chunk data
  """
  def __init__(self, info):
    super(ChunkInfo, self).__init__(info)

class VectorReadInfo(Struct):
  """Vector read response object.
  Returned by :mod:`XRootD.client.File.vector_read()`.

  This object is iterable::

    >>> f.open('root://localhost/tmp/spam')
    >>> status, chunks = file.vector_read([(0, 10), (10, 10)])
    >>> print chunks
    <XRootD.client.responses.VectorReadInfo object at 0x288b9f0>
    >>> print chunks.size
    20
    >>> for chunk in chunks:
    ...   print chunk.offset, chunk.length
    ...
    0 10
    10 10

  :var    size: Total size of all chunks
  :var  chunks: (List of :mod:`XRootD.client.responses.ChunkInfo` objects) -
                The list of chunks that were read
  """
  def __init__(self, info):
    info.update({'chunks': [ChunkInfo(c) for c in info['chunks']]})
    super(VectorReadInfo, self).__init__(info)

  def __iter__(self):
    return iter(self.chunks)

class HostList(Struct):
  """A list of hosts that were involved in the request.

  This object is iterable::

    >>> print hostlist
    <XRootD.client.responses.HostList object at 0x288b9f0>
    >>> for host in hostlist:
    ...   print host.url
    ...
    root://localhost

  :var  hosts: (List of :mod:`XRootD.client.responses.HostInfo` objects) -
               The list of hosts
  """
  def __init__(self, hostlist):
    super(HostList, self).__init__({'hosts': [HostInfo(h) for h in hostlist]})

  def __iter__(self):
    return iter(self.hosts)

class HostInfo(Struct):
  """Information about a single host.

  :var           url: URL of the host, instance of :mod:`XRootD.client.URL`
  :var      protocol: Version of the protocol the host is speaking
  :var         flags: Host type, an `ORed` combination of
                      :mod:`XRootD.client.flags.HostTypes`
  :var load_balancer: Was the host used as a load balancer
  """
  def __init__(self, info):
    super(HostInfo, self).__init__(info)

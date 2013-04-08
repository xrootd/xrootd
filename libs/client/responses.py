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

class XRootDStatus(object):
  """Status of an operation. Returned with every request."""
  def __init__(self, status):
    self.status = status['status']       #: = 0, the status
    self.code = status['code']
    self.errno = status['errNo']
    self.message = status['message']
    self.shellcode = status['shellCode']
    self.error = status['isError']
    self.fatal = status['isFatal']
    self.ok = status['isOK']

  def __str__(self):
    return self.message

class StatInfo(object):
  """Stat information for files and directories"""
  def __init__(self, info):
    self.id = info['id']
    self.flags = info['flags']
    self.size = info['size']
    self.modtime = info['modTime']
    self.modtimestr = info['modTimeAsString']

class StatInfoVFS(object):
  def __init__(self, info):
    self.nodes_rw = info['nodesRW']
    self.free_rw = info['freeRW']
    self.utilization_rw = info['utilizationRW']
    self.nodes_staging = info['nodesStaging']
    self.free_staging = info['freeStaging']
    self.utilization_staging = info['utilizationStaging']

class LocationInfo(object):
  def __init__(self, locations):
    self.locations = list()
    for l in locations:
      self.locations.append(Location(l))

  def __iter__(self):
    return iter(self.locations)

class Location(object):
  def __init__(self, location):
    self.address = location['address']
    self.type = location['type']
    self.accesstype = location['accessType']
    self.is_manager = location['isManager']
    self.is_server = location['isServer']

class HostInfo(object):
  def __init__(self, info):
    self.hosts = list()
    for h in info:
      self.hosts.append(Host(h))

  def __iter__(self):
    return iter(self.hosts)

class Host(object):
  def __init__(self, host):
    self.url = host['url']
    self.protocol = host['protocol']
    self.flags = host['flags']
    self.load_balancer = host['loadBalancer']

class VectorReadInfo(object):
  def __init__(self, info):
    self.size = info['size']
    self.chunks = list()
    for c in info['chunks']:
      self.chunks.append(ChunkInfo(c))

  def __iter__(self):
    return iter(self.chunks)

class ChunkInfo(object):
  def __init__(self, chunk):
    self.offset = chunk['offset']
    self.length = chunk['length']
    self.buffer = chunk['buffer']

class ProtocolInfo(object):
  def __init__(self, info):
    self.version = info['version']
    self.hostinfo = info['hostInfo']

class DirectoryList(object):
  def __init__(self, dirlist):
    self.size = dirlist['size']
    self.parent = dirlist['parent']
    self.dirlist = list()
    for f in dirlist['dirList']:
      self.dirlist.append(ListEntry(f))
      
  def __iter__(self):
    return iter(self.dirlist)

class ListEntry(object):
  def __init__(self, entry):
    self.hostaddr = entry['hostAddress']
    self.name = entry['name']
    if entry['statInfo']: self.statinfo = StatInfo(entry['statInfo'])
    else: self.statinfo = None


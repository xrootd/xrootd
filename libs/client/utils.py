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

from threading import Lock
from XRootD.client.responses import XRootDStatus, HostList

class CallbackWrapper(object):
  def __init__(self, callback, responsetype):
    if not hasattr(callback, '__call__'):
      raise TypeError('callback must be callable function, class or lambda')
    self.callback = callback
    self.responsetype = responsetype

  def __call__(self, status, response, hostlist):
    self.status = XRootDStatus(status)
    self.response = response
    if self.responsetype:
      self.response = self.responsetype(response)
    self.hostlist = HostList(hostlist)
    self.callback(self.status, self.response, self.hostlist)

class AsyncResponseHandler(object):
  """Utility class to handle asynchronous method calls."""
  def __init__(self):
    self.mutex = Lock()
    self.mutex.acquire()

  def __call__(self, status, response, hostlist):
    self.status = status
    self.response = response
    self.hostlist = hostlist
    self.mutex.release()

  def wait(self):
    """Block and wait for the async response"""
    self.mutex.acquire()
    self.mutex.release()
    return self.status, self.response, self.hostlist

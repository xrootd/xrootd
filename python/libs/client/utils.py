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

from threading import Lock
from XRootD.client.responses import XRootDStatus, HostList

class CallbackWrapper(object):
  def __init__(self, callback, responsetype):
    if not hasattr(callback, '__call__'):
      raise TypeError('callback must be callable function, class or lambda')
    self.callback = callback
    self.responsetype = responsetype

  def __call__(self, status, response, *argv):
    self.status = XRootDStatus(status)
    self.response = response
    if self.responsetype and self.response:
      self.response = self.responsetype(response)
    if argv:
      self.hostlist = HostList(argv[0])
    else:
      self.hostlist = HostList([])
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

class CopyProgressHandler(object):
  """Utility class to handle progress updates from copy jobs

  .. note:: This class does nothing by itself. You have to subclass it and do
            something useful with the progress updates yourself.
  """

  def begin(self, jobId, total, source, target):
    """Notify when a new job is about to start

    :param  jobId: the job number of the copy job concerned
    :type   jobId: integer
    :param  total: total number of jobs being processed
    :type   total: integer
    :param source: the source url of the current job
    :type  source: :mod:`XRootD.client.URL` object
    :param target: the destination url of the current job
    :type  target: :mod:`XRootD.client.URL` object
    """
    pass

  def end(self, jobId, results):
    """Notify when the previous job has finished

    :param  jobId: the job number of the copy job concerned
    :type   jobId: integer
    :param status: status of the job
    :type  status: :mod:`XRootD.client.responses.XRootDStatus` object
    """
    pass

  def update(self, jobId, processed, total):
    """Notify about the progress of the current job

    :param     jobId: the job number of the copy job concerned
    :type      jobId: integer
    :param processed: bytes processed by the current job
    :type  processed: integer
    :param     total: total number of bytes to be processed by the current job
    :type      total: integer
    """
    pass


  def should_cancel( self, jobId ):
    """Check whether the current job should be canceled.

    :param  jobId: the job number of the copy job concerned
    :type   jobId: integer
    """
    return False

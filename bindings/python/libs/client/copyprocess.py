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
from XRootD.client.url import URL
from XRootD.client.responses import XRootDStatus

class ProgressHandlerWrapper(object):
  """Internal progress handler wrapper to convert parameters to friendly 
  types"""
  def __init__(self, handler):
    self.handler = handler

  def begin(self, jobId, total, source, target):
    if self.handler:
      self.handler.begin(jobId, total, URL(source), URL(target))

  def end(self, jobId, results):
    if 'status' in results:
      results['status'] = XRootDStatus(results['status'])
    if self.handler:
      self.handler.end(jobId, results)

  def update(self, jobId, processed, total):
    if self.handler:
      self.handler.update(jobId, processed, total)

  def should_cancel(self, jobId):
    if self.handler:
      return self.handler.should_cancel(jobId)
    else:
      return False

class CopyProcess(object):
  """Add multiple individually-configurable copy jobs to a "copy process" and
  run them in parallel (yes, in parallel, because ``xrootd`` isn't limited
  by the `GIL`."""

  def __init__(self):
    self.__process = client.CopyProcess()

  def parallel(self, parallel):
    """ Add a config job to the copy process in order to set the number of
        parallel copy jobs.

    :param parallel: number of parallel copy jobs
    :type  parallel: integer
    """
    self.__process.parallel(parallel)

  def add_job(self,
              source,
              target,
              sourcelimit    = 1,
              force          = False,
              posc           = False,
              coerce         = False,
              mkdir          = False,
              thirdparty     = 'none',
              checksummode   = 'none',
              checksumtype   = '',
              checksumpreset = '',
              dynamicsource  = False,
              chunksize      = 8388608,
              parallelchunks = 4,
              inittimeout    = 600,
              tpctimeout     = 1800,
              rmBadCksum     = False ):
    """Add a job to the copy process.

    :param         source: original source URL
    :type          source: string
    :param         target: target directory or file
    :type          target: string
    :param    sourcelimit: max number of download sources
    :type     sourcelimit: integer
    :param          force: overwrite target if it exists
    :type           force: boolean
    :param           posc: persist on successful close
    :type            posc: boolean
    :param         coerce: ignore file usage rules, i.e. apply `FORCE` flag to
                           ``open()``
    :type          coerce: boolean
    :param          mkdir: create the parent directories when creating a file
    :type           mkdir: boolean
    :param     thirdparty: third party copy mode
    :type      thirdparty: string
    :param   checksummode: checksum mode to be used
    :type    checksummode: string
    :param   checksumtype: type of the checksum to be computed
    :type    checksumtype: string
    :param checksumpreset: pre-set checksum instead of computing it
    :type  checksumpreset: string
    :param  dynamicsource: read as much data from source as is available without
                           checking the size
    :type   dynamicsource: boolean
    :param      chunksize: chunk size for remote transfers
    :type       chunksize: integer
    :param parallelchunks: number of chunks that should be requested in parallel
    :type  parallelchunks: integer
    :param    inittimeout: copy initialization timeout
    :type     inittimeout: integer
    :param     tpctimeout: timeout for a third-party copy to finish
    :type      tpctimeout: integer
    :param     rmBadCksum: remove target file on bad checksum
    :type      rmBadCksum: boolean
    """
    self.__process.add_job(source, target, sourcelimit, force, posc, coerce, mkdir,
                           thirdparty, checksummode, checksumtype, checksumpreset,
                           dynamicsource, chunksize, parallelchunks, inittimeout,
                           tpctimeout, rmBadCksum)

  def prepare(self):
    """Prepare the copy jobs. **Must be called before** ``run()``."""
    status = self.__process.prepare()
    return XRootDStatus(status)

  def run(self, handler=None):
    """Run the copy jobs with an optional progress handler.

    :param handler: a copy progress handler. You can subclass 
                    :mod:`XRootD.client.utils.CopyProgressHandler` and implement
                    the three methods (``begin()``, ``progress()`` and ``end()``
                    ) to get regular progress updates for your copy jobs.
    """
    status, results = self.__process.run(ProgressHandlerWrapper(handler))
    for x in results:
      if 'status' in x:
        x['status'] = XRootDStatus(x['status'])
    return XRootDStatus(status), results

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

from pyxrootd import client
from XRootD.client import URL
from XRootD.client.responses import XRootDStatus

class ProgressHandlerWrapper(object):
  """Internal progress handler wrapper to convert parameters to friendly 
  types"""
  def __init__(self, handler):
    self.handler = handler

  def begin(self, jobId, total, source, target):
    if self.handler:
      self.handler.begin(jobId, total, URL(source), URL(target))

  def end(self, jobId, result):
    if self.handler:
      self.handler.end(jobId, result)

  def update(self, jobId, processed, total):
    if self.handler:
      self.handler.update(jobId, processed, total)

  def should_cancel(self, jobId):
    if self.handler:
      return self.handler.should_cancel(jobId)
    return False

class CopyProcess(object):
  """Add multiple individually-configurable copy jobs to a "copy process" and
  run them in parallel (yes, in parallel, because ``xrootd`` isn't limited
  by the `GIL`."""

  def __init__(self):
    self.__process = client.CopyProcess()

  def add_job(self, source, target, sourcelimit=1, force=False, posc=False,
              coerce=False, makedir=False, thirdparty="none", checksummode="none",
              checksumtype="", checksumpreset="", chunksize=4194304,
              parallelchunks=8, inittimeout=0, tpctimeout=0, dynamicsource=False):
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
    :param        makedir: create missing directory tree for the file
    :type         makedif: boolean
    :param     thirdparty: thirdparty copy mode: "none", "first", "only"
    :type      thirdparty: string
    :param   checksummode: checksumming operations to be performed: "none", "end2end", "source", "target"
    :type    checksummode: string
    :param   checksumtype: type of the checksum to be calculates "md5", "adler32", and so on
    :type    checksumtype: string
    :param checksumpreset: pre-set the value of the source checksum
    :type  checksumpreset: string
    :param      chunksize: chunk size for remote transfers
    :type       chunksize: integer
    :param parallelchunks: number of chunks that should be requested in parallel
    :type  parallelchunks: integer
    :param    inittimeout: copy initialization timeout
    :type     inittimeout: integer
    :param     tpctimeout: timeout for a third-party copy to finish
    :type      tpctimeout: integer
    :param  dynamicsource: allow for the size of the sourcefile to change during copy
    :type   dynamicsource: boolean
    """
    self.__process.add_job(source, target, sourcelimit, force, posc, coerce,
                           makedir, thirdparty, checksummode, checksumtype,
                           checksumpreset, chunksize, parallelchunks,
                           inittimeout, tpctimeout, dynamicsource)

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
    status = self.__process.run(ProgressHandlerWrapper(handler))
    return XRootDStatus(status)

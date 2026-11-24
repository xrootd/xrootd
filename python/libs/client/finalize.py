#-------------------------------------------------------------------------------
# Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
# Author: Michal Simon <michal.simon@cern.ch>
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
#------------------------------------------------------------------------------
from __future__ import absolute_import

import atexit

from pyxrootd import client
from .file import _files


@atexit.register
def finalize():
     """Python atexit handler, will stop all XRootD client threads
        (XrdCl JobManager, TaskManager and Poller) in order to ensure 
        no Python APIs are called after the Python Interpreter gets
        finalized.
     """
     # Close the files which are still open. After the threads stop, the
     # destructor of a file does not send a close, and the server then
     # handles the file as the file of a client which failed. For example,
     # it removes a file which was opened with OpenFlags.POSC. Use only our
     # own File objects: an access to other objects, such as all objects of
     # the garbage collector, can run the code of other packages (#2910).
     # Before Python 3.14, values() reads the dictionary one step at a time,
     # and a File made by a callback thread then raises a RuntimeError.
     # valuerefs() copies the dictionary in one step.
     # A File whose __init__ did not run has no native file.
     for ref in _files.valuerefs():
        f = ref()
        if f is None or getattr(f, '_File__file', None) is None:
            continue
        if f.is_open():
            f.close()

     client.__XrdCl_Stop_Threads()

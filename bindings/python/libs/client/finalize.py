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

import gc
import atexit

from pyxrootd import client
from .file import File


@atexit.register
def finalize():
     """Python atexit handler, will stop all XRootD client threads
        (XrdCl JobManager, TaskManager and Poller) in order to ensure 
        no Python APIs are called after the Python Interpreter gets
        finalized.
     """
     # Ensure there are no files left open as calling their destructor will
     # cause "close" commands to be sent.
     # If this isn't done, there will be no running threads to process requests
     # after this function returns so the interpreter will deadlock.
     for obj in gc.get_objects():
         if isinstance(obj, File) and obj.is_open():
             obj.close()

     client.__XrdCl_Stop_Threads()

# -------------------------------------------------------------------------------
# Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
# Author: Michal Simon <michal.simon@cern.ch>
# ------------------------------------------------------------------------------
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
# ------------------------------------------------------------------------------

# import atexit
from pyxrootd import client

# @atexit.register


def finalize():
    """Python atexit handler, will stop all XRootD client threads
       (XrdCl JobManager, TaskManager and Poller) in order to ensure 
       no Python APIs are called after the Python Interpreter gets
       finalized.
    """
    print(client.anotherFunctionTest())


finalize()

# finalize()

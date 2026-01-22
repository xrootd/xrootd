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

from pyxrootd import client


def EnvPutString( key, value ):
     """Sets the given key in the xrootd client environment to 
        the given value. Returns false if there is already a 
        shell-imported setting for this key, true otherwise
     """
     return client.EnvPutString_cpp( key, value )
 
def EnvGetString( key ):
     """Gets the given key from the xrootd client environment. 
        If key does not exist in the environment returns None.
     """
     return client.EnvGetString_cpp( key )
 
def EnvPutInt( key, value ):
     """Sets the given key in the xrootd client environment to 
        the given value. Returns false if there is already a 
        shell-imported setting for this key, true otherwise
     """
     return client.EnvPutInt_cpp( key, value )
 
def EnvGetInt( key ):
     """Gets the given key from the xrootd client environment. 
        If key does not exist in the environment returns None.
     """
     return client.EnvGetInt_cpp( key )

def EnvGetDefault( key ):
     """ Get the default value for the given key.
         If key does not exist in the environment returns None.
     """
     return client.EnvGetDefault_cpp( key )
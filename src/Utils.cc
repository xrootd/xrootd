//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "Utils.hh"
#include "PyXRootDURL.hh"

namespace PyXRootD
{
  bool IsCallable( PyObject *callable )
  {
    if ( !PyCallable_Check( callable ) ) {
      PyErr_SetString( PyExc_TypeError,
                      "callback must be callable function, class or lambda" );
      return NULL;
    }
    // We need to keep this callback
    Py_INCREF( callable );
    return true;
  }

  int InitTypes()
  {
    URLType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &URLType ) < 0 ) return -1;

    Py_INCREF( &URLType );
    return 0;
  }
}


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

#ifndef UTILS_HH_
#define UTILS_HH_

#include "PyXRootD.hh"

#include "XrdCl/XrdClXRootDResponses.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Check that the given callback is actually callable.
  //----------------------------------------------------------------------------
  bool IsCallable( PyObject *callable );

  //----------------------------------------------------------------------------
  //! Initialize the Python types for the extension.
  //----------------------------------------------------------------------------
  int InitTypes();

  //----------------------------------------------------------------------------
  //! Convert PyInt to unsigned long (uint64_t)
  //!
  //! @param py_val python object to be converted
  //! @param val converted value
  //! @param name name of the object
  //!
  //! return 0 if successful, otherwise failed
  //----------------------------------------------------------------------------
  int PyIntToUlong(PyObject *py_val, unsigned long *val, const char *name);

  //----------------------------------------------------------------------------
  //! Convert Python object to unsigned long (uint64_t)
  //!
  //! @param py_val python object to be converted
  //! @param val converted value
  //! @param name name of the object
  //!
  //! return 0 if successful, otherwise failed
  //----------------------------------------------------------------------------
  int PyObjToUlong(PyObject *py_val, unsigned long *val, const char *name);

  //----------------------------------------------------------------------------
  //! Convert Python object to unsigned int (uint32_t)
  //!
  //! @param py_val python object to be converted
  //! @param val converted value
  //! @param name name of the object
  //!
  //! return 0 if successful, otherwise failed
  //----------------------------------------------------------------------------
  int PyObjToUint(PyObject *py_val, unsigned int *val, const char *name);

  //----------------------------------------------------------------------------
  //! Convert Python object to unsigned short int (uint16_t)
  //!
  //! @param py_val python object to be converted
  //! @param val converted value
  //! @param name name of the object
  //!
  //! return 0 if successful, otherwise failed
  //----------------------------------------------------------------------------
  int PyObjToUshrt(PyObject *py_val, unsigned short int *val, const char *name);

  //----------------------------------------------------------------------------
  //! Convert Python object to unsigned long long (uint64_t)
  //!
  //! @param py_val python object to be converted
  //! @param val converted value
  //! @param name name of the object
  //!
  //! return 0 if successful, otherwise failed
  //----------------------------------------------------------------------------
  int PyObjToUllong(PyObject *py_val, unsigned PY_LONG_LONG *val, const char *name);
}

#endif /* UTILS_HH_ */

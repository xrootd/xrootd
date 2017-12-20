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
  //----------------------------------------------------------------------------
  // Check that the given callback is actually callable.
  //----------------------------------------------------------------------------
  bool IsCallable( PyObject *callable )
  {
    if ( !PyCallable_Check( callable ) ) {
      PyErr_SetString( PyExc_TypeError,
                      "callback must be callable function, class or lambda" );
      return false;
    }
    // We need to keep this callback
    Py_INCREF( callable );
    return true;
  }

  //----------------------------------------------------------------------------
  // Initialize the Python types for the extension.
  //----------------------------------------------------------------------------
  int InitTypes()
  {
    URLType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &URLType ) < 0 ) return -1;

    Py_INCREF( &URLType );
    return 0;
  }

  //----------------------------------------------------------------------------
  // Convert PyInt to unsigned long (uint64_t)
  //----------------------------------------------------------------------------
  int PyIntToUlong(PyObject *py_val, unsigned long *val, const char *name)
  {
#ifdef IS_PY3K
    const long tmp = PyLong_AsLong(py_val);
#else
    const long tmp = PyInt_AsLong(py_val);
#endif

    if (tmp == -1 && PyErr_Occurred())
    {
      if (PyErr_ExceptionMatches(PyExc_OverflowError))
        PyErr_Format(PyExc_OverflowError, "%s too big for unsigned long", name);
      return -1;
    }

    if (tmp < 0)
    {
      PyErr_Format(PyExc_OverflowError,
                   "negative %s cannot be converted to unsigned long", name);
      return -1;
    }

    *val = tmp;
    return 0;
  }

  //----------------------------------------------------------------------------
  // Convert Python object to unsigned long (uint64_t)
  //----------------------------------------------------------------------------
  int PyObjToUlong(PyObject *py_val, unsigned long *val, const char *name)
  {
#ifdef IS_PY3K
    if (PyLong_Check(py_val))
#else
    if (PyInt_Check(py_val))
#endif
      return PyIntToUlong(py_val, val, name);

    if (!PyLong_Check(py_val))
    {
      PyErr_Format(PyExc_TypeError, "expected integer %s", name);
      return -1;
    }

    const unsigned long tmp = PyLong_AsUnsignedLong(py_val);

    if (PyErr_Occurred())
    {
      if (PyErr_ExceptionMatches(PyExc_OverflowError))
        PyErr_Format(PyExc_OverflowError, "%s too big for unsigned long", name);
      return -1;
    }

    *val = tmp;
    return 0;
  }

  //----------------------------------------------------------------------------
  // Convert Python object to unsigned int (uint32_t)
  //----------------------------------------------------------------------------
  int PyObjToUint(PyObject *py_val, unsigned int *val, const char *name)
  {
    unsigned long tmp;

    if (PyObjToUlong(py_val, &tmp, name))
      return -1;

    if (tmp > UINT_MAX)
    {
      PyErr_Format(PyExc_OverflowError, "%s too big for unsigned int (uint32_t)",
                   name);
      return -1;
    }

    *val = tmp;
    return 0;
  }

  //----------------------------------------------------------------------------
  // Convert Python object to unsigned short int (uint16_t)
  //----------------------------------------------------------------------------
  int PyObjToUshrt(PyObject *py_val, unsigned short int *val, const char *name)
  {
    unsigned int tmp;

    if (PyObjToUint(py_val, &tmp, name))
      return -1;

    if (tmp > USHRT_MAX)
    {
      PyErr_Format(PyExc_OverflowError, "%s too big for unsigned short int "
                   "(uint16_t)", name);
      return -1;
    }

    *val = tmp;
    return 0;
  }

  //----------------------------------------------------------------------------
  // Convert Python object to unsigned long long (uint64_t)
  //----------------------------------------------------------------------------
  int PyObjToUllong(PyObject *py_val, unsigned PY_LONG_LONG *val,
                    const char *name)
  {
#ifdef IS_PY3K
    if (PyLong_Check(py_val))
#else
    if (PyInt_Check(py_val))
#endif
    {
      unsigned long tmp;

      if (!PyIntToUlong(py_val, &tmp, name))
      {
        *val = tmp;
        return 0;
      }

      return -1;
    }

    if (!PyLong_Check(py_val))
    {
      PyErr_Format(PyExc_TypeError, "integer argument expected for %s", name);
      return -1;
    }

    const unsigned PY_LONG_LONG tmp = PyLong_AsUnsignedLongLong(py_val);

    if ((tmp == (unsigned long long) -1) && PyErr_Occurred())
    {
      if (PyErr_ExceptionMatches(PyExc_OverflowError))
        PyErr_Format(PyExc_OverflowError,
                     "%s too big for unsigned long long", name);
      return -1;
    }

    *val = tmp;
    return 0;
  }
}

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

#ifndef PYXROOTD_HH_
#define PYXROOTD_HH_

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string>
#include "structmember.h"

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#define Py_TPFLAGS_HAVE_ITER 0
#else
#define PyUnicode_AsUTF8             PyString_AsString
#define PyUnicode_Check              PyString_Check
#define PyUnicode_FromString         PyString_FromString
#define PyUnicode_FromStringAndSize  PyString_FromStringAndSize
#define PyUnicode_GET_LENGTH         PyString_Size
#endif

#define async( func )    \
  Py_BEGIN_ALLOW_THREADS \
  func;                  \
  Py_END_ALLOW_THREADS   \

#endif /* PYXROOTD_HH_ */

//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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
#ifndef BINDINGS_PYTHON_SRC_PYXROOTDFINALIZE_HH_
#define BINDINGS_PYTHON_SRC_PYXROOTDFINALIZE_HH_

#include "PyXRootD.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPostMaster.hh"



#include <iostream>
#include <string>
namespace PyXRootD
{

//----------------------------------------------------------------------------
//! Waits until XrdCl JobManager, TaskManager and Poller will stop gracefully.
//!
//! To be used in Python native atexit handler in order to make sure there are
//! no outstanding threads after the Python interpreter got finalized.
//----------------------------------------------------------------------------
PyObject *__XrdCl_Stop_Threads(PyObject *self, PyObject *)
{
  // Py_BEGIN_ALLOW_THREADS
  // XrdCl::DefaultEnv::GetPostMaster()->Stop();
  // Py_END_ALLOW_THREADS
  // Py_RETURN_NONE;
  PyObject *result = NULL;
  std::string name = "robert";
  result = Py_BuildValue("s", name.c_str());
  return result;
}

PyObject *anotherFunctionTest(PyObject *self)
{
  auto number = 69.6969;
  return Py_BuildValue("i", static_cast<int>(number));
}

//trying to open and close a file. Output to python
PyObject *tryOpenFile(PyObject *self)
{
  XrdCl::File f;
  std::string filepath = "root://localhost//tmp/myfile.txt";
  std::cout << "opening..."
            << "\n";
  // auto p1 = File::Open(f, filepath, XrdCl::OpenFlags::Read);
  std::cout << "closing..."
            << "\n";
  // auto p3 = File::Close(f);
  std::string message1 = "The pipelines finished...";
  return Py_BuildValue("s", message1.c_str());
}
} // namespace PyXRootD

#endif /* BINDINGS_PYTHON_SRC_PYXROOTDFINALIZE_HH_ */

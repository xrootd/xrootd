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

#include "PyXRootD.hh"
#include "PyXRootDFileSystem.hh"
#include "PyXRootDFile.hh"
#include "PyXRootDCopyProcess.hh"
#include "PyXRootDURL.hh"

namespace PyXRootD
{
  // Global module object
  PyObject* ClientModule;

  PyDoc_STRVAR(client_module_doc, "XRootD Client extension module");

  //----------------------------------------------------------------------------
  //! Visible module-level method declarations
  //----------------------------------------------------------------------------
  static PyMethodDef module_methods[] =
    {
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Module initialization function
  //----------------------------------------------------------------------------
  PyMODINIT_FUNC initclient( void )
  {
    // Ensure GIL state is initialized
    Py_Initialize();
    if ( !PyEval_ThreadsInitialized() ) {
      PyEval_InitThreads();
    }

    FileSystemType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &FileSystemType ) < 0 ) return;
    Py_INCREF( &FileSystemType );

    FileType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &FileType ) < 0 ) return;
    Py_INCREF( &FileType );

    URLType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &URLType ) < 0 ) return;
    Py_INCREF( &URLType );

    CopyProcessType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &CopyProcessType ) < 0 ) return;
    Py_INCREF( &CopyProcessType );

    ClientModule = Py_InitModule3("client", module_methods, client_module_doc);

    if (ClientModule == NULL) {
      return;
    }

    PyModule_AddObject( ClientModule, "FileSystem", (PyObject *) &FileSystemType );
    PyModule_AddObject( ClientModule, "File", (PyObject *) &FileType );
    PyModule_AddObject( ClientModule, "URL", (PyObject *) &URLType );
    PyModule_AddObject( ClientModule, "CopyProcess", (PyObject *) &CopyProcessType );
  }
}

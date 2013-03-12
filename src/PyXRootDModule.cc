//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#include "PyXRootD.hh"
#include "PyXRootDClient.hh"

namespace PyXRootD
{
  // Global module object
  PyObject* ClientModule;

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

    ClientType.tp_new   = PyType_GenericNew;
    if ( PyType_Ready( &ClientType ) < 0 )   return;
    Py_INCREF( &ClientType );

    ClientModule = Py_InitModule3("client", module_methods,
        "XRootD Client extension module.");

    if (ClientModule == NULL) {
      return;
    }

    PyModule_AddObject( ClientModule, "Client", (PyObject *) &ClientType );
  }
}

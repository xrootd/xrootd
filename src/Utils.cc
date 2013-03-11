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

#include "Utils.hh"

namespace PyXRootD
{
  template<class Type>
  PyObject* ConvertType( Type *type, PyTypeObject *bindType )
  {
    PyObject *args = Py_BuildValue( "(O)",
        PyCObject_FromVoidPtr( (void *) type, NULL ) );
    if ( !args )
      return NULL;

    bindType->tp_new = PyType_GenericNew;
    if ( PyType_Ready( bindType ) < 0 ) {
      return NULL;
    }

    PyObject *bind = PyObject_CallObject( (PyObject *) bindType, args );
    Py_DECREF( args );
    if ( !bind )
      return NULL;

    return bind;
  }

  PyObject* XRootDStatusDict( XrdCl::XRootDStatus *status )
  {
    PyObject *dict = Py_BuildValue( "{sHsHsIsssisOsOsO}",
        "status",    status->status,
        "code",      status->code,
        "errNo",     status->errNo,
        "message",   status->ToStr().c_str(),
        "shellCode", status->GetShellCode(),
        "isError",   PyBool_FromLong( status->IsError() ),
        "isFatal",   PyBool_FromLong( status->IsFatal() ),
        "isOK",      PyBool_FromLong( status->IsOK() ) );

    return (dict == NULL || PyErr_Occurred()) ? NULL : dict;
  }

  bool IsCallable( PyObject *callable )
  {
    if ( !PyCallable_Check( callable ) ) {
      PyErr_SetString( PyExc_TypeError, "parameter must be callable" );
      return NULL;
    }
    // We need to keep this callback
    Py_INCREF( callable );
    return true;
  }

  int InitTypes()
  {
    //ClientType.tp_new   = PyType_GenericNew;
    URLType.tp_new      = PyType_GenericNew;
    StatInfoType.tp_new = PyType_GenericNew;
    HostInfoType.tp_new = PyType_GenericNew;

    //if ( PyType_Ready( &ClientType ) < 0 )   return -1;
    if ( PyType_Ready( &URLType ) < 0 )      return -1;
    if ( PyType_Ready( &StatInfoType ) < 0 ) return -1;
    if ( PyType_Ready( &HostInfoType ) < 0 ) return -1;

    //Py_INCREF( &ClientType );
    Py_INCREF( &URLType );
    Py_INCREF( &StatInfoType );
    Py_INCREF( &HostInfoType );

    return 0;
  }
}


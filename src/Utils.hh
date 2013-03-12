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

#ifndef UTILS_HH_
#define UTILS_HH_

#include "PyXRootD.hh"
#include "PyXRootDType.hh"
#include "PyXRootDURL.hh"

#include "XrdCl/XrdClXRootDResponses.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Convert a C++ type to its corresponding Python binding type. We cast
  //! the object to a void * before packing it into a PyCObject.
  //!
  //! Note: The PyCObject API is deprecated as of Python 2.7
  //----------------------------------------------------------------------------
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

  template<typename T>
  struct dict;

  template<>
  struct dict<XrdCl::AnyObject>
  {
      static PyObject* convert( XrdCl::AnyObject *object )
      {
        return Py_BuildValue( "{}" );
      }
  };

  template<>
  struct dict<XrdCl::XRootDStatus>
  {
      static PyObject* convert( XrdCl::XRootDStatus *status )
      {
        return Py_BuildValue( "{sHsHsIsssisOsOsO}",
            "status",    status->status,
            "code",      status->code,
            "errNo",     status->errNo,
            "message",   status->ToStr().c_str(),
            "shellCode", status->GetShellCode(),
            "isError",   PyBool_FromLong( status->IsError() ),
            "isFatal",   PyBool_FromLong( status->IsFatal() ),
            "isOK",      PyBool_FromLong( status->IsOK() ) );
      }
  };

  template<>
  struct dict<XrdCl::StatInfo>
  {
      static PyObject* convert( XrdCl::StatInfo *info )
      {
        return Py_BuildValue( "{sssksIskss}",
            "id",               info->GetId().c_str(),
            "size",             info->GetSize(),
            "flags",            info->GetFlags(),
            "modTime",          info->GetModTime(),
            "modTimeAsString",  info->GetModTimeAsString().c_str() );
      }
  };

  template<>
  struct dict<XrdCl::HostInfo>
  {
      static PyObject* convert( XrdCl::HostInfo *info )
      {
        return Py_BuildValue( "{sIsIsOsO}",
            "flags",        info->flags,
            "protocol",     info->protocol,
            "loadBalancer", PyBool_FromLong(info->loadBalancer),
            "url",          ConvertType<XrdCl::URL>(&info->url, &URLType));
      }
  };

  template<typename Type>
  PyObject* ConvertType( Type *type )
  {
    return dict<Type>::convert(type);
  }

  //----------------------------------------------------------------------------
  //! Check that the given callback is actually callable.
  //----------------------------------------------------------------------------
  bool IsCallable( PyObject *callable );

  //----------------------------------------------------------------------------
  //! Initialize the Python types for the extension.
  //----------------------------------------------------------------------------
  int InitTypes();
}

#endif /* UTILS_HH_ */

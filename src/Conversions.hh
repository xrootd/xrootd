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

#ifndef CONVERSIONS_HH_
#define CONVERSIONS_HH_

#include "PyXRootDURL.hh"
#include "Utils.hh"

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

  template<typename T> struct PyDict;

  template<typename Type>
  PyObject* ConvertType( Type *type )
  {
    return PyDict<Type>::Convert(type);
  }

  template<> struct PyDict<XrdCl::AnyObject>
  {
      static PyObject* Convert( XrdCl::AnyObject *object )
      {
        Py_RETURN_NONE;
      }
  };

  template<typename T>
  PyObject* ConvertResponse( T *response )
  {
    if ( response ) {
      return ConvertType<T>( response );
    } else {
      Py_RETURN_NONE;
    }
  }

  template<> struct PyDict<XrdCl::XRootDStatus>
  {
      static PyObject* Convert( XrdCl::XRootDStatus *status )
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

  template<> struct PyDict<XrdCl::ProtocolInfo>
  {
      static PyObject* Convert( XrdCl::ProtocolInfo *info )
      {
        return Py_BuildValue( "{sIsI}",
            "version",  info->GetVersion(),
            "hostInfo", info->GetHostInfo() );
      }
  };

  template<> struct PyDict<XrdCl::StatInfo>
  {
      static PyObject* Convert( XrdCl::StatInfo *info )
      {
        return Py_BuildValue( "{sssksIskss}",
            "id",               info->GetId().c_str(),
            "size",             info->GetSize(),
            "flags",            info->GetFlags(),
            "modTime",          info->GetModTime(),
            "modTimeAsString",  info->GetModTimeAsString().c_str() );
      }
  };

  template<> struct PyDict<XrdCl::StatInfoVFS>
  {
      static PyObject* Convert( XrdCl::StatInfoVFS *info )
      {
        return Py_BuildValue( "{sksksksksbsb}",
            "nodesRW",            info->GetNodesRW(),
            "nodesStaging",       info->GetNodesStaging(),
            "freeRW",             info->GetFreeRW(),
            "freeStaging",        info->GetFreeStaging(),
            "utilizationRW",      info->GetUtilizationRW(),
            "utilizationStaging", info->GetUtilizationStaging() );
      }
  };

  template<> struct PyDict<XrdCl::DirectoryList>
  {
      static PyObject* Convert( XrdCl::DirectoryList *list )
      {
        PyObject *directoryList = PyList_New( 0 );
        PyObject *statInfo;
        for ( XrdCl::DirectoryList::Iterator i = list->Begin(); i < list->End();
            ++i ) {
          statInfo = ConvertResponse<XrdCl::StatInfo>( (*i)->GetStatInfo() );
          PyList_Append( directoryList,
              Py_BuildValue( "{sssssO}",
                  "hostAddress", (*i)->GetHostAddress().c_str(),
                  "name",        (*i)->GetName().c_str(),
                  "statInfo",    statInfo ) );
        }
        return Py_BuildValue( "{sisssO}",
            "size",     list->GetSize(),
            "parent",   list->GetParentName().c_str(),
            "dirList",  directoryList );
      }
  };

  template<> struct PyDict<XrdCl::HostList>
  {
      static PyObject* Convert( XrdCl::HostList *list )
      {
        URLType.tp_new = PyType_GenericNew;
        if ( PyType_Ready( &URLType ) < 0 ) return NULL;
        Py_INCREF( &URLType );

        PyObject *pyhostlist = PyList_New( 0 );

        if ( list ) {
          for ( unsigned int i = 0; i < list->size(); ++i ) {
            XrdCl::HostInfo *info = &list->at( i );
            std::cout << ">>>>> " << info;
            PyObject *url = PyObject_CallObject( (PyObject*) &URLType,
                Py_BuildValue( "(s)", info->url.GetURL().c_str() ) );
            _PyObject_Dump(url);
            PyObject *pyhostinfo = Py_BuildValue( "{sIsIsOsO}",
                "flags",        info->flags,
                "protocol",     info->protocol,
                "loadBalancer", PyBool_FromLong(info->loadBalancer),
                "url",          url );

            Py_INCREF( pyhostinfo );
            PyList_Append( pyhostlist, pyhostinfo );
          }
        }

        return pyhostlist;
      }
  };

  template<> struct PyDict<XrdCl::LocationInfo>
  {
      static PyObject* Convert( XrdCl::LocationInfo *info )
      {
        PyObject *locationList = PyList_New( 0 );
        for ( XrdCl::LocationInfo::Iterator i = info->Begin(); i < info->End();
            ++i ) {
          PyList_Append( locationList,
              Py_BuildValue( "{sssIsIsOsO}",
                  "address",    i->GetAddress().c_str(),
                  "type",       i->GetType(),
                  "accessType", i->GetAccessType(),
                  "isServer",   PyBool_FromLong( i->IsServer() ),
                  "isManager",  PyBool_FromLong( i->IsManager() ) ) );
        }
        return Py_BuildValue( "O", locationList );
      }
  };

  template<> struct PyDict<XrdCl::Buffer>
  {
      static PyObject* Convert( XrdCl::Buffer *buffer )
      {
        return Py_BuildValue( "s#", buffer->GetBuffer(), buffer->GetSize() );
      }
  };

  template<> struct PyDict<XrdCl::VectorReadInfo>
  {
      static PyObject* Convert( XrdCl::VectorReadInfo *info )
      {
        if ( !info ) return Py_BuildValue( "" );

        PyObject        *pychunks = PyList_New( 0 );
        XrdCl::ChunkList chunks   = info->GetChunks();

        for ( uint32_t i = 0; i < chunks.size(); ++i ) {
          XrdCl::ChunkInfo chunk  = chunks.at( i );

          PyList_Append( pychunks,
              Py_BuildValue( "{sksIsO}",
                  "offset", chunk.offset,
                  "length", chunk.length,
                  "buffer", Py_BuildValue( "s#", (const char *) chunk.buffer,
                                           chunk.length ) ) );
        }
        return Py_BuildValue( "{sIsO}",
            "size",   info->GetSize(),
            "chunks", pychunks );
      }
  };
}

#endif /* CONVERSIONS_HH_ */

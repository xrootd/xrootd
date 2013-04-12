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
  template<typename T> struct PyDict;

  template<typename T>
  PyObject* ConvertType( T *response )
  {
    if ( response ) {
      return PyDict<T>::Convert( response );
    } else {
      Py_RETURN_NONE;
    }
  }

  template<> struct PyDict<XrdCl::AnyObject>
  {
      static PyObject* Convert( XrdCl::AnyObject *object )
      {
        Py_RETURN_NONE;
      }
  };

  template<> struct PyDict<XrdCl::XRootDStatus>
  {
      static PyObject* Convert( XrdCl::XRootDStatus *status )
      {
        return Py_BuildValue( "{sHsHsIsssisOsOsO}",
            "status",    status->status,
            "code",      status->code,
            "errno",     status->errNo,
            "message",   status->ToStr().c_str(),
            "shellcode", status->GetShellCode(),
            "error",     PyBool_FromLong( status->IsError() ),
            "fatal",     PyBool_FromLong( status->IsFatal() ),
            "ok",        PyBool_FromLong( status->IsOK() ) );
      }
  };

  template<> struct PyDict<XrdCl::ProtocolInfo>
  {
      static PyObject* Convert( XrdCl::ProtocolInfo *info )
      {
        return Py_BuildValue( "{sIsI}",
            "version",  info->GetVersion(),
            "hostinfo", info->GetHostInfo() );
      }
  };

  template<> struct PyDict<XrdCl::StatInfo>
  {
      static PyObject* Convert( XrdCl::StatInfo *info )
      {
        return Py_BuildValue( "{sssksIskss}",
            "id",         info->GetId().c_str(),
            "size",       info->GetSize(),
            "flags",      info->GetFlags(),
            "modtime",    info->GetModTime(),
            "modtimestr", info->GetModTimeAsString().c_str() );
      }
  };

  template<> struct PyDict<XrdCl::StatInfoVFS>
  {
      static PyObject* Convert( XrdCl::StatInfoVFS *info )
      {
        return Py_BuildValue( "{sksksksksbsb}",
            "nodes_rw",            info->GetNodesRW(),
            "nodes_staging",       info->GetNodesStaging(),
            "free_rw",             info->GetFreeRW(),
            "free_staging",        info->GetFreeStaging(),
            "utilization_rw",      info->GetUtilizationRW(),
            "utilization_staging", info->GetUtilizationStaging() );
      }
  };

  template<> struct PyDict<XrdCl::DirectoryList>
  {
      static PyObject* Convert( XrdCl::DirectoryList *list )
      {
        PyObject *directoryList = PyList_New( list->GetSize() );
        PyObject *statInfo;
        int i = 0;

        for ( XrdCl::DirectoryList::Iterator it = list->Begin();
              it < list->End(); ++it ) {
          statInfo = ConvertType<XrdCl::StatInfo>( (*it)->GetStatInfo() );

          PyList_SET_ITEM( directoryList, i,
              Py_BuildValue( "{sssssO}",
                  "hostaddr", (*it)->GetHostAddress().c_str(),
                  "name",     (*it)->GetName().c_str(),
                  "statinfo", statInfo ) );
          Py_DECREF( statInfo );
          i++;
        }

        PyObject *o = Py_BuildValue( "{sisssO}",
            "size",     list->GetSize(),
            "parent",   list->GetParentName().c_str(),
            "dirlist",  directoryList );
        Py_DECREF( directoryList );
        return o;
      }
  };

  template<> struct PyDict<XrdCl::HostList>
  {
      static PyObject* Convert( XrdCl::HostList *list )
      {
        URLType.tp_new = PyType_GenericNew;
        if ( PyType_Ready( &URLType ) < 0 ) return NULL;
        Py_INCREF( &URLType );

        PyObject *pyhostlist = PyList_New( list->size() );

        if ( list ) {
          for ( unsigned int i = 0; i < list->size(); ++i ) {
            XrdCl::HostInfo *info = &list->at( i );

            PyObject *url = PyObject_CallObject( (PyObject*) &URLType,
                Py_BuildValue( "(s)", info->url.GetURL().c_str() ) );

            PyObject *pyhostinfo = Py_BuildValue( "{sIsIsOsO}",
                "flags",         info->flags,
                "protocol",      info->protocol,
                "load_balancer", PyBool_FromLong(info->loadBalancer),
                "url",           url );

            Py_DECREF( url );
            PyList_SET_ITEM( pyhostlist, i, pyhostinfo );
          }
        }

        return pyhostlist;
      }
  };

  template<> struct PyDict<XrdCl::LocationInfo>
  {
      static PyObject* Convert( XrdCl::LocationInfo *info )
      {
        PyObject *locationList = PyList_New( info->GetSize() );
        int i = 0;

        for ( XrdCl::LocationInfo::Iterator it = info->Begin(); it < info->End();
            ++it ) {
          PyList_SET_ITEM( locationList, i,
              Py_BuildValue( "{sssIsIsOsO}",
                  "address",     it->GetAddress().c_str(),
                  "type",        it->GetType(),
                  "accesstype",  it->GetAccessType(),
                  "is_server",   PyBool_FromLong( it->IsServer() ),
                  "is_manager",  PyBool_FromLong( it->IsManager() ) ) );
          i++;
        }

        PyObject *o = Py_BuildValue( "O", locationList );
        Py_DECREF( locationList );
        return o;
      }
  };

  template<> struct PyDict<XrdCl::Buffer>
  {
      static PyObject* Convert( XrdCl::Buffer *buffer )
      {
        return Py_BuildValue( "s#", buffer->GetBuffer(), buffer->GetSize() );
      }
  };

  template<> struct PyDict<XrdCl::ChunkInfo>
  {
      static PyObject* Convert( XrdCl::ChunkInfo *chunk )
      {
        PyObject *o = Py_BuildValue( "s#", chunk->buffer, chunk->length );
        delete[] (char*) chunk->buffer;
        return o;
      }
  };

  template<> struct PyDict<XrdCl::VectorReadInfo>
  {
      static PyObject* Convert( XrdCl::VectorReadInfo *info )
      {
        if ( !info ) return Py_BuildValue( "" );

        XrdCl::ChunkList chunks   = info->GetChunks();
        PyObject        *pychunks = PyList_New( chunks.size() );

        for ( uint32_t i = 0; i < chunks.size(); ++i ) {
          XrdCl::ChunkInfo chunk = chunks.at( i );

          PyObject *buffer = Py_BuildValue( "s#", (const char *) chunk.buffer,
                                                  chunk.length );
          PyList_SET_ITEM( pychunks, i,
              Py_BuildValue( "{sksIsO}",
                  "offset", chunk.offset,
                  "length", chunk.length,
                  "buffer", buffer ) );
          Py_DECREF( buffer );
        }

        PyObject *o = Py_BuildValue( "{sIsO}", "size", info->GetSize(),
                                               "chunks", pychunks );
        Py_DECREF( pychunks );
        return o;
      }
  };
}

#endif /* CONVERSIONS_HH_ */

//------------------------------------------------------------------------------
// Copyright (c) 2012-2015 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#ifndef CONVERSIONS_HH_
#define CONVERSIONS_HH_

#include "PyXRootDURL.hh"
#include "Utils.hh"
#include <deque>

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClPropertyList.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Convert an object of type T into a Python dictionary type.
  //----------------------------------------------------------------------------
  template<typename T> struct PyDict;

  template<typename T>
  inline PyObject* ConvertType( T *response )
  {
    if ( response != NULL ) {
      return PyDict<T>::Convert( response );
    } else {
      Py_RETURN_NONE;
    }
  }

  template<>
  inline PyObject* ConvertType<const std::deque<XrdCl::PropertyList> >(const std::deque<XrdCl::PropertyList> *list )
  {
    if(list == NULL)
      Py_RETURN_NONE;

    PyObject *pylist = NULL;

    if(list)
    {
      pylist = PyList_New(list->size());
      std::deque<XrdCl::PropertyList>::const_iterator it = list->begin();
      for(unsigned int i = 0; i < list->size(); ++i)
      {
        const XrdCl::PropertyList &result = *it++;
        PyObject *pyresult = ConvertType(&result);
        PyList_SetItem(pylist, i, pyresult);
      }
    }

    return pylist;
  }

  template<>
  inline PyObject* ConvertType<std::deque<XrdCl::PropertyList> >(std::deque<XrdCl::PropertyList> *list )
  {
    return ConvertType((const std::deque<XrdCl::PropertyList>*) list);
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
        PyObject *error = PyBool_FromLong(status->IsError());
        PyObject *fatal = PyBool_FromLong(status->IsFatal());
        PyObject *ok    = PyBool_FromLong(status->IsOK());
        PyObject *obj   =
          Py_BuildValue( "{sHsHsIsssisOsOsO}",
            "status",    status->status,
            "code",      status->code,
            "errno",     status->errNo,
            "message",   status->ToStr().c_str(),
            "shellcode", status->GetShellCode(),
            "error",     error,
            "fatal",     fatal,
            "ok",        ok);
        Py_DECREF(error);
        Py_DECREF(fatal);
        Py_DECREF(ok);
        return obj;
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
        return Py_BuildValue("{sOsOsOsOsO}",
            "id",         Py_BuildValue("s", info->GetId().c_str()),
            "size",       Py_BuildValue("k", info->GetSize()),
            "flags",      Py_BuildValue("I", info->GetFlags()),
            "modtime",    Py_BuildValue("k", info->GetModTime()),
            "modtimestr", Py_BuildValue("s",
                                        info->GetModTimeAsString().c_str()));
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

        PyObject *pyhostlist = NULL;

        if ( list ) {
          pyhostlist = PyList_New( list->size() );
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
        return PyBytes_FromStringAndSize( buffer->GetBuffer(), buffer->GetSize() );
      }
  };

  template<> struct PyDict<XrdCl::ChunkInfo>
  {
      static PyObject* Convert( XrdCl::ChunkInfo *chunk )
      {
        PyObject *o = PyBytes_FromStringAndSize( (const char*)chunk->buffer,
                                                  chunk->length );
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

          PyObject *buffer = PyBytes_FromStringAndSize( (const char *) chunk.buffer,
                                                        chunk.length );
          PyList_SET_ITEM( pychunks, i,
              Py_BuildValue( "{sOsOsO}",
                  "offset", Py_BuildValue( "k", chunk.offset ),
                  "length", Py_BuildValue( "I", chunk.length ),
                  "buffer", buffer ) );
          Py_DECREF( buffer );
        }

        PyObject *o = Py_BuildValue( "{sIsO}", "size", info->GetSize(),
                                               "chunks", pychunks );
        Py_DECREF( pychunks );
        return o;
      }
  };

  template<> struct PyDict<const XrdCl::PropertyList>
  {
      static PyObject* Convert(const XrdCl::PropertyList *result)
      {
          PyObject   *pyresult = PyDict_New();
          PyObject   *kO       = 0;
          PyObject   *vO       = 0;
          const char *key      = "sourceCheckSum";

          if(result->HasProperty(key))
          {
            std::string s;
            result->Get(key, s);
            kO = Py_BuildValue("s", key);
            vO = Py_BuildValue("s", s.c_str());
            PyDict_SetItem(pyresult, kO, vO);
            Py_DECREF(kO);
            Py_DECREF(vO);
          }

          key = "targetCheckSum";
          if(result->HasProperty(key))
          {
            std::string s;
            result->Get(key, s);
            kO = Py_BuildValue("s", key);
            vO = Py_BuildValue("s", s.c_str());
            PyDict_SetItem(pyresult, kO, vO);
            Py_DECREF(kO);
            Py_DECREF(vO);
          }

          key = "size";
          if(result->HasProperty(key))
          {
            uint64_t s;
            result->Get(key, s);
            kO = Py_BuildValue("s", key);
            vO = Py_BuildValue("K", s);
            PyDict_SetItem(pyresult, kO, vO);
            Py_DECREF(kO);
            Py_DECREF(vO);

          }

          key = "status";
          if(result->HasProperty(key))
          {
            XrdCl::XRootDStatus s;
            result->Get(key, s);
            kO = Py_BuildValue("s", key);
            vO = ConvertType(&s);
            PyDict_SetItem(pyresult, kO, vO);
            Py_DECREF(kO);
            Py_DECREF(vO);

          }

          key = "sources";
          if(result->HasProperty(key))
          {
            std::vector<std::string> s;
            result->Get(key, s);
            kO = Py_BuildValue("s", key);
            vO = ConvertType(&s);
            PyDict_SetItem(pyresult, kO, vO);
            Py_DECREF(kO);
            Py_DECREF(vO);

          }

          key = "realTarget";
          if(result->HasProperty(key))
          {
            std::string s;
            result->Get(key, s);
            kO = Py_BuildValue("s", key);
            vO = Py_BuildValue("s", s.c_str());
            PyDict_SetItem(pyresult, kO, vO);
            Py_DECREF(kO);
            Py_DECREF(vO);
          }

          return pyresult;
      }
  };

  template<> struct PyDict<std::vector<std::string> >
  {
      static PyObject* Convert( std::vector<std::string> *list )
      {
        PyObject *pylist = NULL;

        if(list)
        {
          pylist = PyList_New(list->size());
          for(unsigned int i = 0; i < list->size(); ++i)
          {
            std::string &str = list->at(i);
            PyList_SetItem(pylist, i, Py_BuildValue("s", str.c_str()));
          }
        }
        return pylist;
      }
  };

}

#endif /* CONVERSIONS_HH_ */

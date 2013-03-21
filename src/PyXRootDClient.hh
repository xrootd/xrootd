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

#ifndef CLIENT_TYPE_HH_
#define CLIENT_TYPE_HH_

#include "PyXRootD.hh"
#include "PyXRootDURL.hh"
#include "PyXRootDDocumentation.hh"
#include "Conversions.hh"

#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Client binding type definition
  //----------------------------------------------------------------------------
  typedef struct
  {
      PyObject_HEAD
      /* Type-specific fields */
      URL *url;
      XrdCl::FileSystem *filesystem;
  } Client;

  //----------------------------------------------------------------------------
  //! XrdCl::FileSystem binding class
  //----------------------------------------------------------------------------
  class FileSystem
  {
    public:
      static PyObject* Locate( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* DeepLocate( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Mv( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Query( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Truncate( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Rm( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* MkDir( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* RmDir( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* ChMod( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Ping( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Stat( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* StatVFS( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Protocol( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* DirList( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* SendInfo( Client *self, PyObject *args, PyObject *kwds );
      static PyObject* Prepare( Client *self, PyObject *args, PyObject *kwds );
  };

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef ClientMethods[] =
    {
      { "locate",     (PyCFunction) PyXRootD::FileSystem::Locate,
          METH_KEYWORDS, filesystem_locate_doc},
      { "deeplocate", (PyCFunction) PyXRootD::FileSystem::DeepLocate,
          METH_KEYWORDS, filesystem_deeplocate_doc },
      { "mv",         (PyCFunction) PyXRootD::FileSystem::Mv,
          METH_KEYWORDS, filesystem_mv_doc },
      { "query",      (PyCFunction) PyXRootD::FileSystem::Query,
          METH_KEYWORDS, filesystem_query_doc },
      { "truncate",   (PyCFunction) PyXRootD::FileSystem::Truncate,
          METH_KEYWORDS, filesystem_truncate_doc },
      { "rm",         (PyCFunction) PyXRootD::FileSystem::Rm,
          METH_KEYWORDS, filesystem_rm_doc },
      { "mkdir",      (PyCFunction) PyXRootD::FileSystem::MkDir,
          METH_KEYWORDS, filesystem_mkdir_doc },
      { "rmdir",      (PyCFunction) PyXRootD::FileSystem::RmDir,
          METH_KEYWORDS, filesystem_rmdir_doc },
      { "chmod",      (PyCFunction) PyXRootD::FileSystem::ChMod,
          METH_KEYWORDS, filesystem_chmod_doc },
      { "ping",       (PyCFunction) PyXRootD::FileSystem::Ping,
          METH_KEYWORDS, filesystem_ping_doc },
      { "stat",       (PyCFunction) PyXRootD::FileSystem::Stat,
          METH_KEYWORDS, filesystem_stat_doc },
      { "statvfs",    (PyCFunction) PyXRootD::FileSystem::StatVFS,
          METH_KEYWORDS, filesystem_statvfs_doc },
      { "protocol",   (PyCFunction) PyXRootD::FileSystem::Protocol,
          METH_KEYWORDS, filesystem_protocol_doc },
      { "dirlist",    (PyCFunction) PyXRootD::FileSystem::DirList,
          METH_KEYWORDS, filesystem_dirlist_doc },
      { "sendinfo",   (PyCFunction) PyXRootD::FileSystem::SendInfo,
          METH_KEYWORDS, filesystem_sendinfo_doc },
      { "prepare",    (PyCFunction) PyXRootD::FileSystem::Prepare,
          METH_KEYWORDS, filesystem_prepare_doc },
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int Client_init( Client *self, PyObject *args, PyObject *kwds )
  {
    const char *urlstr;
    static char *kwlist[] = { "url", NULL };

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s", kwlist, &urlstr ) )
      return -1;

    XrdCl::URL *url = new XrdCl::URL( urlstr );
    self->url = (URL *) ConvertType<XrdCl::URL>( url, &URLType );

    if ( !self->url )
      return NULL;

    self->filesystem = new XrdCl::FileSystem( *url );

    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void Client_dealloc( Client *self )
  {
    Py_XDECREF( self->url );
    self->ob_type->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef ClientMembers[] =
    {
      { "url", T_OBJECT_EX, offsetof(Client, url), 0, "Server URL" },
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Client binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject ClientType =
    { PyObject_HEAD_INIT(NULL) 0,               /* ob_size */
    "client.Client",                            /* tp_name */
    sizeof(Client),                             /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) Client_dealloc,                /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    client_type_doc,                            /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    ClientMethods,                              /* tp_methods */
    ClientMembers,                              /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) Client_init,                     /* tp_init */
  };
}

#endif /* CLIENT_TYPE_HH_ */

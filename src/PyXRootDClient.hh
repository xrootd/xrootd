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
#include "Utils.hh"

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
  } Client;

  //----------------------------------------------------------------------------
  //! XrdCl::FileSystem binding class
  //----------------------------------------------------------------------------
  class FileSystem
  {
    public:
      static PyObject* Stat( Client *self, PyObject *args );
      static PyObject* Ping( Client *self, PyObject *args, PyObject *kwds );
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
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef ClientMethods[] =
    {
      { "stat", (PyCFunction) PyXRootD::FileSystem::Stat, METH_KEYWORDS,
        "Stat a path" },
      { "ping", (PyCFunction) PyXRootD::FileSystem::Ping, METH_KEYWORDS,
        "Check if the server is alive" },
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
    "Client object",                            /* tp_doc */
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

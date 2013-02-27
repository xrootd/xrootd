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

#ifndef XRDCLBIND_HH_
#define XRDCLBIND_HH_

#include <Python.h>
#include <iostream>
#include "structmember.h"

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XRootDStatusType.hh"
#include "StatInfoType.hh"
#include "URLType.hh"
#include "AsyncResponseHandler.hh"
#include "XrdClBindUtils.hh"

namespace XrdClBind
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
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void Client_dealloc( Client *self )
  {
    Py_XDECREF( self->url );
    self->ob_type->tp_free( (PyObject*) self );
  }

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
  //! Stat a path.
  //!
  //! This function can be synchronous or asynchronous, depending if a callback
  //! argument is given. The callback can be any Python callable.
  //----------------------------------------------------------------------------
  static PyObject* Stat( Client *self, PyObject *args )
  {
    const char *path;
    PyObject   *callback = NULL;
    XrdCl::XRootDStatus status;
    XrdCl::FileSystem   filesystem( *self->url->url );

    //--------------------------------------------------------------------------
    // Parse the stat path and optional callback argument
    //--------------------------------------------------------------------------
    if ( !PyArg_ParseTuple( args, "s|O", &path, &callback ) )
      return NULL;

    //--------------------------------------------------------------------------
    // Asynchronous mode
    //--------------------------------------------------------------------------
    if ( callback ) {
      if (!CheckCallable(callback)) return NULL;

      XrdCl::ResponseHandler *handler =
          new AsyncResponseHandler<XrdCl::StatInfo>( &StatInfoType, callback );

      //------------------------------------------------------------------------
      // Spin the async request (while releasing the GIL) and return None.
      //------------------------------------------------------------------------
      Py_BEGIN_ALLOW_THREADS
      status = filesystem.Stat( path, handler, 5 );
      Py_END_ALLOW_THREADS

      PyObject *statusDict = XRootDStatusDict(&status);
      if (!statusDict) return NULL;
      return Py_BuildValue( "O", statusDict );
    }

    //--------------------------------------------------------------------------
    // Synchronous mode
    //--------------------------------------------------------------------------
    XrdCl::StatInfo *response = 0;
    status = filesystem.Stat( path, response, 5 );

    //--------------------------------------------------------------------------
    // Convert the XRootDStatus object
    //--------------------------------------------------------------------------
    PyObject *statusDict = XRootDStatusDict(&status);
    if (!statusDict) return NULL;

    //--------------------------------------------------------------------------
    // Convert the response object, if any
    //--------------------------------------------------------------------------
    PyObject *responseBind;
    if ( response ) {
      responseBind = ConvertType<XrdCl::StatInfo>( response, &StatInfoType );
    } else {
      responseBind = Py_None;
    }

    return Py_BuildValue( "OO", statusDict, responseBind );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive
  //----------------------------------------------------------------------------
  static PyObject* Ping( Client *self, PyObject *args, PyObject *kwds )
  {
    uint16_t timeout = 5;
    PyObject *callback = NULL;
    XrdCl::XRootDStatus status;
    XrdCl::FileSystem   filesystem( *self->url->url );
    static char *kwlist[] = { "timeout", "callback", NULL };

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|IO", kwlist, &timeout, &callback ) )
      return NULL;

    if ( callback ) {
      if ( !CheckCallable( callback ) )
        return NULL;

      XrdCl::ResponseHandler *handler =
          new AsyncResponseHandler<>( (PyTypeObject*) Py_None, callback );

      Py_BEGIN_ALLOW_THREADS
      status = filesystem.Ping( handler, timeout );
      Py_END_ALLOW_THREADS

      PyObject *statusDict = XRootDStatusDict( &status );
      if ( !statusDict )
        return NULL;
      return Py_BuildValue( "O", statusDict );
    }

    status = filesystem.Ping( timeout );

    PyObject *statusDict = XRootDStatusDict( &status );
    if ( !statusDict )
      return NULL;

    return Py_BuildValue( "O", statusDict );
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
      { "stat", (PyCFunction) Stat, METH_KEYWORDS,
        "Stat a path" },
      { "ping", (PyCFunction) Ping, METH_KEYWORDS,
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
  };}

#endif /* XRDCLBIND_HH_ */

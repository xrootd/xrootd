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
  static void Client_dealloc(Client *self)
  {
    Py_XDECREF(self->url);
    self->ob_type->tp_free((PyObject*) self);
  }

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int Client_init(Client *self, PyObject *args, PyObject *kwds)
  {
    const char *urlstr;
    static char *kwlist[] = { "url", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &urlstr))
      return -1;

    PyObject *bindArgs = Py_BuildValue("(s)", urlstr);
    if (!bindArgs) return NULL;

    //--------------------------------------------------------------------------
    // Create ourselves a binding object for the URL
    //--------------------------------------------------------------------------
    self->url = (URL*) PyObject_CallObject((PyObject*) &URLType, bindArgs);
    Py_DECREF(bindArgs);
    if (!self->url) return NULL;

    return 0;
  }

  //----------------------------------------------------------------------------
  //! Stat a path.
  //!
  //! This function can be synchronous or asynchronous, depending if a callback
  //! argument is given. The callback can be any Python callable.
  //----------------------------------------------------------------------------
  static PyObject* Stat(Client *self, PyObject *args)
  {
    const char *path;
    PyObject *callback = NULL;

    //--------------------------------------------------------------------------
    // Parse the stat path and optional callback argument
    //--------------------------------------------------------------------------
    if (!PyArg_ParseTuple(args, "s|O", &path, &callback))
      return NULL;

    XrdCl::FileSystem fs(*self->url->url);

    //--------------------------------------------------------------------------
    // Asynchronous mode
    //--------------------------------------------------------------------------
    if (callback) {
      //------------------------------------------------------------------------
      // Check that the given callback is actually callable.
      //------------------------------------------------------------------------
      if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
      }
      // We need to keep this callback inside the response handler
      Py_INCREF(callback);

      XrdCl::ResponseHandler *handler = new AsyncResponseHandler<XrdCl::StatInfo>
                                            (&StatInfoType, callback);

      //------------------------------------------------------------------------
      // Spin the async request (while releasing the GIL) and return None.
      //------------------------------------------------------------------------
      Py_BEGIN_ALLOW_THREADS
      fs.Stat(path, handler, 5);
      Py_END_ALLOW_THREADS

      Py_RETURN_NONE;
    }

    //--------------------------------------------------------------------------
    // Synchronous mode
    //--------------------------------------------------------------------------
    XrdCl::XRootDStatus status;
    XrdCl::StatInfo *response = 0;
    status = fs.Stat(path, response, 5);

    //--------------------------------------------------------------------------
    // Convert the XRootDStatus object
    //--------------------------------------------------------------------------
    PyObject *statusArgs = Py_BuildValue("(HHIs)", status.status,
            status.code, status.errNo, status.GetErrorMessage().c_str());
    if (!statusArgs) return NULL;

    PyObject *statusBind = PyObject_CallObject(
                                (PyObject *) &XRootDStatusType, statusArgs);
    if (!statusBind) return NULL;
    Py_DECREF(statusArgs);

    //--------------------------------------------------------------------------
    // Convert the response object, if any
    //--------------------------------------------------------------------------
    PyObject *responseBind;
    if (response) {

      //------------------------------------------------------------------------
      // The CObject API is deprecated as of Python 2.7
      //------------------------------------------------------------------------
      PyObject *responseArgs = Py_BuildValue("(O)",
                               PyCObject_FromVoidPtr((void *) response, NULL));
      if (!responseArgs) return NULL;

      //------------------------------------------------------------------------
      // Call the constructor of the bound type.
      //------------------------------------------------------------------------
      responseBind = PyObject_CallObject((PyObject *) &StatInfoType,
                                         responseArgs);
    } else {
      responseBind = Py_None;
    }

    return Py_BuildValue("OO", statusBind, responseBind);
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
      { "stat", (PyCFunction) Stat, METH_VARARGS, "Stat a path" },
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

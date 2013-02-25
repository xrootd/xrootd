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

#ifndef HOSTINFOTYPE_HH_
#define HOSTINFOTYPE_HH_

#include <Python.h>
#include "structmember.h"

#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdClBind
{
  //----------------------------------------------------------------------------
  //! HostInfo binding type definition
  //----------------------------------------------------------------------------
  typedef struct
  {
      PyObject_HEAD
      /* Type-specific fields */
      XrdCl::HostInfo *hostInfo;
  } HostInfo;

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void HostInfo_dealloc(HostInfo *self)
  {
    delete self->hostInfo;
    self->ob_type->tp_free((PyObject*) self);
  }

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int HostInfo_init(HostInfo *self, PyObject *args)
  {
    PyObject *hostInfo;

    if (!PyArg_ParseTuple(args, "O", &hostInfo))
      return -1;

    self->hostInfo = (XrdCl::HostInfo *) PyCObject_AsVoidPtr(hostInfo);
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Binding functions
  //----------------------------------------------------------------------------
  static PyObject* HostInfo_GetFlags(HostInfo *self, void *closure)
  {
    return Py_BuildValue("i", self->hostInfo->flags);
  }

  static PyObject* HostInfo_GetProtocol(HostInfo *self, void *closure)
  {
    return Py_BuildValue("i", self->hostInfo->protocol);
  }

  static PyObject* HostInfo_IsLoadBalancer(HostInfo *self, void *closure)
  {
    return Py_BuildValue("O", PyBool_FromLong(self->hostInfo->loadBalancer));
  }

  static PyObject* HostInfo_GetURL(HostInfo *self, void *closure)
  {
    //--------------------------------------------------------------------------
    // Build a URL mapping object on-the-fly (maybe inefficient)
    //--------------------------------------------------------------------------
    PyObject *bindArgs = Py_BuildValue("(s)", self->hostInfo->url.GetURL().c_str());
    if (!bindArgs) return NULL;

    PyObject *url = PyObject_CallObject((PyObject*) &URLType, bindArgs);
    Py_DECREF(bindArgs);
    if (!url) return NULL;

    return url;
  }

  //----------------------------------------------------------------------------
  //! Custom getter/setter function declarations
  //----------------------------------------------------------------------------
  static PyGetSetDef HostInfoGetSet[] =
    {
      { "flags", (getter) HostInfo_GetFlags, NULL,
        "Host type", NULL},
      { "protocol", (getter) HostInfo_GetProtocol, NULL,
        "Version of the protocol the host is speaking", NULL},
      { "loadBalancer", (getter) HostInfo_IsLoadBalancer, NULL,
        "Was the host used as a load balancer", NULL},
      { "url", (getter) HostInfo_GetURL, NULL,
        "URL of the host", NULL},
      { NULL} /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef HostInfoMembers[] =
    {
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef HostInfoMethods[] =
    {
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! HostInfo binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject HostInfoType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "client.HostInfo",                          /* tp_name */
    sizeof(HostInfo),                           /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) HostInfo_dealloc,              /* tp_dealloc */
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
    "HostInfo object",                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    HostInfoMethods,                            /* tp_methods */
    HostInfoMembers,                            /* tp_members */
    HostInfoGetSet,                             /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) HostInfo_init,                   /* tp_init */
  };
}

#endif /* HOSTINFOTYPE_HH_ */

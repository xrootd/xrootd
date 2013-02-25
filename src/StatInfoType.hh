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

#ifndef STATINFOTYPE_HH_
#define STATINFOTYPE_HH_

#include <Python.h>

#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdClBind
{
  //----------------------------------------------------------------------------
  //! StatInfo binding type definition
  //----------------------------------------------------------------------------
  typedef struct
  {
      PyObject_HEAD
      /* Type-specific fields */
      XrdCl::StatInfo *info;
  } StatInfo;

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void StatInfo_dealloc(StatInfo *self)
  {
    self->ob_type->tp_free((PyObject*) self);
  }

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int StatInfo_init(StatInfo *self, PyObject *args, PyObject *kwds)
  {
    static char *kwlist[] = { "data", NULL };
    PyObject *data;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &data))
      return -1;

    // Unpack the void * and cast it back to our object
    self->info = (XrdCl::StatInfo *) PyCObject_AsVoidPtr(data);
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Wrapper functions for underlying XrdCl::StatInfo
  //----------------------------------------------------------------------------
  static PyObject* GetId(StatInfo *self)
  {
    return Py_BuildValue("S", PyString_FromString(self->info->GetId().c_str()));
  }

  static PyObject* GetSize(StatInfo *self)
  {
    return Py_BuildValue("K", PyLong_FromLongLong(self->info->GetSize()));
  }

  static PyObject* GetFlags(StatInfo *self)
  {
    return Py_BuildValue("k", PyLong_FromLong(self->info->GetFlags()));
  }

  static PyObject* TestFlags(StatInfo *self, PyObject *args)
  {
    uint32_t flags;
    if (!PyArg_ParseTuple(args, "", &flags))
      return NULL;
    return Py_BuildValue("O", PyBool_FromLong(self->info->TestFlags(flags)));
  }

  static PyObject* GetModTime(StatInfo *self)
  {
    return Py_BuildValue("K", PyLong_FromLongLong(self->info->GetModTime()));
  }

  static PyObject* GetModTimeAsString(StatInfo *self)
  {
    return Py_BuildValue("S",
        PyString_FromString(self->info->GetModTimeAsString().c_str()));
  }

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef StatInfoMembers[] =
    {
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef StatInfoMethods[] = {
    { "GetId", (PyCFunction) GetId, METH_NOARGS, "Get ID" },
    { "GetSize", (PyCFunction) GetSize, METH_NOARGS, "Get size (in bytes)" },
    { "GetFlags", (PyCFunction) GetFlags, METH_NOARGS, "Get flags" },
    { "TestFlags", (PyCFunction) TestFlags, METH_VARARGS, "Test flags" },
    { "GetModTime", (PyCFunction) GetModTime, METH_NOARGS,
        "Get modification time (in seconds since epoch)" },
    { "GetModTimeAsString", (PyCFunction) GetModTimeAsString, METH_NOARGS,
        "Get modification time" },
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! StatInfo binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject StatInfoType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "client.StatInfo",                          /* tp_name */
    sizeof(StatInfo),                           /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) StatInfo_dealloc,              /* tp_dealloc */
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
    "StatInfo object",                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    StatInfoMethods,                            /* tp_methods */
    StatInfoMembers,                            /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) StatInfo_init,                   /* tp_init */
  };
}

#endif /* STATINFOTYPE_HH_ */

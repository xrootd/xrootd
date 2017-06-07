//------------------------------------------------------------------------------
// Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
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

#ifndef PYXROOTD_COPY_PROCESS_HH_
#define PYXROOTD_COPY_PROCESS_HH_

#include "PyXRootD.hh"

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include <deque>

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! XrdCl::CopyProcess binding class
  //----------------------------------------------------------------------------
  class CopyProcess
  {
    public:
      static PyObject* AddJob(CopyProcess *self, PyObject *args, PyObject *kwds);
      static PyObject* Prepare(CopyProcess *self, PyObject *args, PyObject *kwds);
      static PyObject* Run(CopyProcess *self, PyObject *args, PyObject *kwds);
    public:
      PyObject_HEAD
      XrdCl::CopyProcess              *process;
      std::deque<XrdCl::PropertyList> *results;
  };

  PyDoc_STRVAR(copyprocess_type_doc, "CopyProcess object (internal)");

  //----------------------------------------------------------------------------
  //! __init__()
  //----------------------------------------------------------------------------
  static int CopyProcess_init( CopyProcess *self, PyObject *args )
  {
    self->process = new XrdCl::CopyProcess();
    self->results = new std::deque<XrdCl::PropertyList>();
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void CopyProcess_dealloc( CopyProcess *self )
  {
    delete self->process;
    delete self->results;
    Py_TYPE(self)->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef CopyProcessMethods[] =
  {
    { "add_job",
       (PyCFunction) PyXRootD::CopyProcess::AddJob,  METH_KEYWORDS, NULL },
    { "prepare",
       (PyCFunction) PyXRootD::CopyProcess::Prepare, METH_KEYWORDS, NULL },
    { "run",
       (PyCFunction) PyXRootD::CopyProcess::Run,     METH_KEYWORDS, NULL },

    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef CopyProcessMembers[] =
  {
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! CopyProcess binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject CopyProcessType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pyxrootd.CopyProcess",                     /* tp_name */
    sizeof(CopyProcess),                        /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) CopyProcess_dealloc,           /* tp_dealloc */
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
    copyprocess_type_doc,                       /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    CopyProcessMethods,                         /* tp_methods */
    CopyProcessMembers,                         /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) CopyProcess_init,                /* tp_init */
  };
}

#endif /* PYXROOTD_COPY_PROCESS_HH_ */

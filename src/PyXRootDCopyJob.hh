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

#ifndef PYXROOTD_COPY_JOB_HH_
#define PYXROOTD_COPY_JOB_HH_

#include "PyXRootD.hh"
#include "PyXRootDURL.hh"
#include "XrdCl/XrdClCopyProcess.hh"

namespace PyXRootD
{
  class CopyJob
  {
    public:
      PyObject_HEAD
      XrdCl::JobDescriptor *job;
  };

  PyDoc_STRVAR(copyjob_type_doc, "CopyJob object (internal)");

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int CopyJob_init( CopyJob *self, PyObject *args )
  {
    const char *source;
    const char *target;

    if ( !PyArg_ParseTuple( args, "ss", &source, &target ) ) return -1;

    if ( !source ) return -1;
    if ( !target ) return -1;

    self->job = new XrdCl::JobDescriptor();
    self->job->source = std::string( source );
    self->job->target = std::string( target );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void CopyJob_dealloc( CopyJob *self )
  {
    delete self->job;
    self->ob_type->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef CopyJobMethods[] =
  {
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef CopyJobMembers[] =
  {
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! CopyJob binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject CopyJobType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "pyxrootd.CopyJob",                         /* tp_name */
    sizeof(CopyJob),                            /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) CopyJob_dealloc,               /* tp_dealloc */
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
    copyjob_type_doc,                           /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    CopyJobMethods,                             /* tp_methods */
    CopyJobMembers,                             /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) CopyJob_init,                    /* tp_init */
  };
}
#endif /* PYXROOTD_COPY_JOB_HH_ */

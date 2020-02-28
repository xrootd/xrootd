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

#ifndef PYXROOTDFILE_HH_
#define PYXROOTDFILE_HH_

#include "PyXRootD.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFile.hh"

#include <deque>

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! XrdCl::File binding class
  //----------------------------------------------------------------------------
  class File
  {
    public:
      static PyObject* Open( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Close( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Stat( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Read( File *self, PyObject *args, PyObject *kwds );
      static PyObject* ReadLine( File *self, PyObject *args, PyObject *kwds );
      static PyObject* ReadLines( File *self, PyObject *args, PyObject *kwds );
      static XrdCl::Buffer* ReadChunk( File *self, uint64_t offset, uint32_t size );
      static PyObject* ReadChunks( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Write( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Sync( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Truncate( File *self, PyObject *args, PyObject *kwds );
      static PyObject* VectorRead( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Fcntl( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Visa( File *self, PyObject *args, PyObject *kwds );
      static PyObject* IsOpen( File *self, PyObject *args, PyObject *kwds );
      static PyObject* GetProperty( File *self, PyObject *args, PyObject *kwds );
      static PyObject* SetProperty( File *self, PyObject *args, PyObject *kwds );
      static PyObject* SetXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* GetXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* DelXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* ListXAttr( File *self, PyObject *args, PyObject *kwds );
    public:
      PyObject_HEAD
      XrdCl::File                *file;
      uint64_t                    currentOffset;
  };

  PyDoc_STRVAR(file_type_doc, "File object (internal)");

  //----------------------------------------------------------------------------
  //! Set exception and return null if I/O op on closed file is attempted
  //----------------------------------------------------------------------------
  static PyObject* FileClosedError()
  {
    PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int File_init( File *self, PyObject *args )
  {
    self->file    = new XrdCl::File();
    self->currentOffset = 0;
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void File_dealloc( File *self )
  {
    delete self->file;
    Py_TYPE(self)->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! __iter__
  //----------------------------------------------------------------------------
  static PyObject* File_iter( File *self )
  {
    if ( !self->file->IsOpen() ) return FileClosedError();

    //--------------------------------------------------------------------------
    // Return ourselves for iteration
    //--------------------------------------------------------------------------
    Py_INCREF( self );
    return (PyObject*) self;
  }

  //----------------------------------------------------------------------------
  //! __iternext__
  //----------------------------------------------------------------------------
  static PyObject* File_iternext( File *self )
  {
    if ( !self->file->IsOpen() ) return FileClosedError();

    PyObject *line = PyObject_CallMethod( (PyObject*) self,
                                          const_cast<char*>("readline"), NULL );
    if( !line ) return NULL;
    //--------------------------------------------------------------------------
    // Raise StopIteration if the line we just read is empty
    //--------------------------------------------------------------------------
    if ( PyBytes_Size( line ) == 0 ) {
      PyErr_SetNone( PyExc_StopIteration );
      return NULL;
    }

    return line;
  }

  //----------------------------------------------------------------------------
  //! __enter__
  //----------------------------------------------------------------------------
  static PyObject* File_enter( File *self )
  {
    Py_INCREF( self );
    return (PyObject*) self;
  }

  //----------------------------------------------------------------------------
  //! __exit__
  //----------------------------------------------------------------------------
  static PyObject* File_exit( File *self )
  {
    PyObject *ret = PyObject_CallMethod( (PyObject*) self,
                                         const_cast<char*>("close"), NULL );
    if ( !ret ) return NULL;
    Py_DECREF( ret );
    Py_RETURN_NONE ;
  }

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef FileMethods[] =
  {
    { "open",
       (PyCFunction) PyXRootD::File::Open,                METH_VARARGS | METH_KEYWORDS, NULL },
    { "close",
       (PyCFunction) PyXRootD::File::Close,               METH_VARARGS | METH_KEYWORDS, NULL },
    { "stat",
       (PyCFunction) PyXRootD::File::Stat,                METH_VARARGS | METH_KEYWORDS, NULL },
    { "read",
       (PyCFunction) PyXRootD::File::Read,                METH_VARARGS | METH_KEYWORDS, NULL },
    { "readline",
       (PyCFunction) PyXRootD::File::ReadLine,            METH_VARARGS | METH_KEYWORDS, NULL },
    { "readlines",
       (PyCFunction) PyXRootD::File::ReadLines,           METH_VARARGS | METH_KEYWORDS, NULL },
    { "readchunks",
       (PyCFunction) PyXRootD::File::ReadChunks,          METH_VARARGS | METH_KEYWORDS, NULL },
    { "write",
       (PyCFunction) PyXRootD::File::Write,               METH_VARARGS | METH_KEYWORDS, NULL },
    { "sync",
       (PyCFunction) PyXRootD::File::Sync,                METH_VARARGS | METH_KEYWORDS, NULL },
    { "truncate",
       (PyCFunction) PyXRootD::File::Truncate,            METH_VARARGS | METH_KEYWORDS, NULL },
    { "vector_read",
       (PyCFunction) PyXRootD::File::VectorRead,          METH_VARARGS | METH_KEYWORDS, NULL },
    { "fcntl",
       (PyCFunction) PyXRootD::File::Fcntl,               METH_VARARGS | METH_KEYWORDS, NULL },
    { "visa",
       (PyCFunction) PyXRootD::File::Visa,                METH_VARARGS | METH_KEYWORDS, NULL },
    { "is_open",
       (PyCFunction) PyXRootD::File::IsOpen,              METH_VARARGS | METH_KEYWORDS, NULL },
    { "get_property",
       (PyCFunction) PyXRootD::File::GetProperty,         METH_VARARGS | METH_KEYWORDS, NULL },
    { "set_property",
       (PyCFunction) PyXRootD::File::SetProperty,         METH_VARARGS | METH_KEYWORDS, NULL },
    { "set_xattr",
       (PyCFunction) PyXRootD::File::SetXAttr,            METH_VARARGS | METH_KEYWORDS, NULL },
    { "get_xattr",
       (PyCFunction) PyXRootD::File::GetXAttr,            METH_VARARGS | METH_KEYWORDS, NULL },
    { "del_xattr",
       (PyCFunction) PyXRootD::File::DelXAttr,            METH_VARARGS | METH_KEYWORDS, NULL },
    { "list_xattr",
       (PyCFunction) PyXRootD::File::ListXAttr,           METH_VARARGS | METH_KEYWORDS, NULL },
    {"__enter__",
       (PyCFunction) File_enter,                          METH_NOARGS,   NULL},
    {"__exit__",
       (PyCFunction) File_exit,                           METH_VARARGS,  NULL},

    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef FileMembers[] =
  {
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! File binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject FileType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pyxrootd.File",                            /* tp_name */
    sizeof(File),                               /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) File_dealloc,                  /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE
    | Py_TPFLAGS_HAVE_ITER,                     /* tp_flags */
    file_type_doc,                              /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    (getiterfunc)  File_iter,                   /* tp_iter */
    (iternextfunc) File_iternext,               /* tp_iternext */
    FileMethods,                                /* tp_methods */
    FileMembers,                                /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) File_init,                       /* tp_init */
  };
}

#endif /* PYXROOTDFILE_HH_ */

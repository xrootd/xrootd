//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
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

#ifndef CHUNKITERATOR_HH_
#define CHUNKITERATOR_HH_

#include "PyXRootD.hh"
#include "PyXRootDFile.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Iterator class for looping over a file and yielding chunks from it
  //----------------------------------------------------------------------------
  class ChunkIterator
  {
    public:
      PyObject_HEAD
      File    *file;
      uint32_t chunksize;
      uint64_t startOffset;
      uint64_t currentOffset;
  };

  //----------------------------------------------------------------------------
  //! __init__
  //----------------------------------------------------------------------------
  static int ChunkIterator_init(ChunkIterator *self, PyObject *args)
  {
    PyObject *py_offset = NULL, *py_chunksize = NULL;

    if ( !PyArg_ParseTuple( args, "OOO", &self->file,&py_offset,
                            &py_chunksize ) ) return -1;

    unsigned long long tmp_offset = 0;
    unsigned int tmp_chunksize = 2 * 1024 * 1024; // 2 MB

    if ( py_offset && PyObjToUllong( py_offset, &tmp_offset, "offset" ) )
      return -1;

    if ( py_chunksize && PyObjToUint( py_chunksize, &tmp_chunksize, "chunksize" ) )
      return -1;

    self->startOffset = (uint64_t)tmp_offset;
    self->chunksize = (uint32_t)tmp_chunksize;
    self->currentOffset = self->startOffset;
    return 0;
  }

  //----------------------------------------------------------------------------
  //! __iter__
  //----------------------------------------------------------------------------
  static PyObject* ChunkIterator_iter(ChunkIterator *self)
  {
    Py_INCREF(self);
    return (PyObject*) self;
  }

  //----------------------------------------------------------------------------
  //! __iternext__
  //!
  //! Dumb implementation, should use prefetching/readv
  //----------------------------------------------------------------------------
  static PyObject* ChunkIterator_iternext(ChunkIterator *self)
  {
    XrdCl::Buffer *chunk = self->file->ReadChunk( self->file,
                                                  self->currentOffset,
                                                  self->chunksize);
    PyObject *pychunk = NULL;

    if ( chunk->GetSize() == 0 ) {
      //------------------------------------------------------------------------
      // Raise StopIteration exception when we are done
      //------------------------------------------------------------------------
      PyErr_SetNone( PyExc_StopIteration );
    }

    else {
      self->currentOffset += self->chunksize;
      pychunk = PyBytes_FromStringAndSize( (const char*) chunk->GetBuffer(),
                                                         chunk->GetSize() );
    }

    delete chunk;
    return pychunk;
  }

  //----------------------------------------------------------------------------
  //! ChunkIterator type structure
  //----------------------------------------------------------------------------
  static PyTypeObject ChunkIteratorType = {
      PyVarObject_HEAD_INIT(NULL, 0)
      "client.File.ChunkIterator",                /* tp_name */
      sizeof(ChunkIterator),                      /* tp_basicsize */
      0,                                          /* tp_itemsize */
      0,                                          /* tp_dealloc */
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
      Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,  /* tp_flags */
      "Internal chunk iterator object",           /* tp_doc */
      0,                                          /* tp_traverse */
      0,                                          /* tp_clear */
      0,                                          /* tp_richcompare */
      0,                                          /* tp_weaklistoffset */
      (getiterfunc)  ChunkIterator_iter,          /* tp_iter */
      (iternextfunc) ChunkIterator_iternext,      /* tp_iternext */
      0,                                          /* tp_methods */
      0,                                          /* tp_members */
      0,                                          /* tp_getset */
      0,                                          /* tp_base */
      0,                                          /* tp_dict */
      0,                                          /* tp_descr_get */
      0,                                          /* tp_descr_set */
      0,                                          /* tp_dictoffset */
      (initproc) ChunkIterator_init,              /* tp_init */
  };
}

#endif /* CHUNKITERATOR_HH_ */

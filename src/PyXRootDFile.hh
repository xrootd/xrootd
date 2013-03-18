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

#ifndef PYXROOTDFILE_HH_
#define PYXROOTDFILE_HH_

#include "PyXRootD.hh"
#include "PyXRootDClient.hh"

#include "XrdCl/XrdClFile.hh"

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
      static PyObject* Readline( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Readlines( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Readchunks( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Write( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Sync( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Truncate( File *self, PyObject *args, PyObject *kwds );
      static PyObject* VectorRead( File *self, PyObject *args, PyObject *kwds );

      static PyObject* IsOpen( File *self, PyObject *args, PyObject *kwds );
      static PyObject* EnableReadRecovery( File *self, PyObject *args, PyObject *kwds );
      static PyObject* EnableWriteRecovery( File *self, PyObject *args, PyObject *kwds );
      static PyObject* GetDataServer( File *self, PyObject *args, PyObject *kwds );

    public:
      PyObject_HEAD
      XrdCl::File *file;
      uint64_t pCurrentOffset;
  };

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
    self->file = new XrdCl::File();
    self->pCurrentOffset = 0;
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void File_dealloc( File *self )
  {
    //delete self->file;
    self->ob_type->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! __iter__
  //----------------------------------------------------------------------------
  static PyObject* File_iter( File *self )
  {
    if ( !self->file->IsOpen() ) return FileClosedError();

    Py_INCREF( self );
    return (PyObject*) self;
  }

  //----------------------------------------------------------------------------
  //! __iternext__
  //----------------------------------------------------------------------------
  static PyObject* File_iternext( File *self )
  {
    if ( !self->file->IsOpen() ) return FileClosedError();

    // Set inital blocksize
    //
    // while read(blocksize) length > 0
    //


    //--------------------------------------------------------------------------
    // Fetch a 4k chunk
    // Probably should read a larger chunk (256k) and split it
    //--------------------------------------------------------------------------
    uint64_t blocksize = 4096;
    PyObject *line = self->Read( self,
        Py_BuildValue( "kI", self->pCurrentOffset, blocksize ), NULL );



    if ( !PyString_Size( PyTuple_GetItem( line, 1 ) ) ) {
      //------------------------------------------------------------------------
      // Raise standard StopIteration exception with empty value
      //------------------------------------------------------------------------
      PyErr_SetNone( PyExc_StopIteration );
      return NULL;
    }

    self->pCurrentOffset += blocksize;
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
    PyObject *ret = PyObject_CallMethod( (PyObject*) self, "close", NULL );
    if ( !ret )
      return NULL;

    Py_DECREF( ret );
    Py_RETURN_NONE ;

  }

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef FileMethods[] =
  {
    { "open",
        (PyCFunction) PyXRootD::File::Open,                METH_KEYWORDS,
        "Open the file pointed to by the given URL" },
    { "close",
        (PyCFunction) PyXRootD::File::Close,               METH_KEYWORDS,
        "Close the file" },
    { "stat",
        (PyCFunction) PyXRootD::File::Stat,                METH_KEYWORDS,
        "Obtain status information for this file" },
    { "read",
        (PyCFunction) PyXRootD::File::Read,                METH_KEYWORDS,
        "Read a data chunk at a given offset" },
    { "readline",
        (PyCFunction) PyXRootD::File::Readline,            METH_KEYWORDS,
        "Read a data chunk at a given offset, until "
        "the first newline encountered" },
    { "readlines",
        (PyCFunction) PyXRootD::File::Readlines,           METH_KEYWORDS,
        "Read data chunks from a given offset, separated "
        "by newlines, until EOF encountered. Return list "
        "of lines read." },
    { "readchunks",
        (PyCFunction) PyXRootD::File::Readchunks,          METH_KEYWORDS,
        "Read data chunks from a given offset of the "
        "given size, until EOF encountered. Return list "
        "of chunks read." },
    { "write",
        (PyCFunction) PyXRootD::File::Write,               METH_KEYWORDS,
        "Write a data chunk at a given offset" },
    { "sync",
        (PyCFunction) PyXRootD::File::Sync,                METH_KEYWORDS,
        "Commit all pending disk writes" },
    { "truncate",
        (PyCFunction) PyXRootD::File::Truncate,            METH_KEYWORDS,
        "Truncate the file to a particular size" },
    { "vector_read",
        (PyCFunction) PyXRootD::File::VectorRead,          METH_KEYWORDS,
        "Read scattered data chunks in one operation" },
    { "is_open",
        (PyCFunction) PyXRootD::File::IsOpen,              METH_KEYWORDS,
        "Check if the file is open" },
    { "enable_read_recovery",
        (PyCFunction) PyXRootD::File::EnableReadRecovery,  METH_KEYWORDS,
        "Enable/disable state recovery procedures while "
        "the file is open for reading" },
    { "enable_write_recovery",
        (PyCFunction) PyXRootD::File::EnableWriteRecovery, METH_KEYWORDS,
        "Enable/disable state recovery procedures while "
        "the file is open for writing or read/write" },
    { "get_data_server",
        (PyCFunction) PyXRootD::File::GetDataServer,       METH_KEYWORDS,
        "Get the data server the file is accessed at" },
    {"__enter__",
        (PyCFunction) File_enter,                           METH_NOARGS,
        "__enter__() -> self."},
    {"__exit__",
        (PyCFunction) File_exit,                           METH_VARARGS,
        "__exit__(*excinfo) -> None.  Closes the file."},

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
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "client.File",                              /* tp_name */
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
    "File object",                              /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    (getiterfunc) File_iter,                    /* tp_iter */
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

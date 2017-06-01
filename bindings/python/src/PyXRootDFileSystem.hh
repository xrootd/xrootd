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

#ifndef PYXROOTD_FILESYSTEM_HH_
#define PYXROOTD_FILESYSTEM_HH_

#include "PyXRootD.hh"
#include "PyXRootDURL.hh"
#include "Conversions.hh"

#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! XrdCl::FileSystem binding class
  //----------------------------------------------------------------------------
  class FileSystem
  {
    public:
      static PyObject* Copy( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Locate( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* DeepLocate( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Mv( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Query( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Truncate( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Rm( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* MkDir( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* RmDir( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* ChMod( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Ping( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Stat( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* StatVFS( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Protocol( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* DirList( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* SendInfo( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* Prepare( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* GetProperty( FileSystem *self, PyObject *args, PyObject *kwds );
      static PyObject* SetProperty( FileSystem *self, PyObject *args, PyObject *kwds );

    public:
      PyObject_HEAD
      URL *url;
      XrdCl::FileSystem *filesystem;
  };

  PyDoc_STRVAR(filesystem_type_doc, "FileSystem object (internal)");

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef FileSystemMethods[] =
    {
      { "copy",
          (PyCFunction) PyXRootD::FileSystem::Copy,       METH_KEYWORDS, NULL },
      { "locate",
          (PyCFunction) PyXRootD::FileSystem::Locate,     METH_KEYWORDS, NULL },
      { "deeplocate",
          (PyCFunction) PyXRootD::FileSystem::DeepLocate, METH_KEYWORDS, NULL },
      { "mv",
          (PyCFunction) PyXRootD::FileSystem::Mv,         METH_KEYWORDS, NULL },
      { "query",
          (PyCFunction) PyXRootD::FileSystem::Query,      METH_KEYWORDS, NULL },
      { "truncate",
          (PyCFunction) PyXRootD::FileSystem::Truncate,   METH_KEYWORDS, NULL },
      { "rm",
          (PyCFunction) PyXRootD::FileSystem::Rm,         METH_KEYWORDS, NULL },
      { "mkdir",
          (PyCFunction) PyXRootD::FileSystem::MkDir,      METH_KEYWORDS, NULL },
      { "rmdir",
          (PyCFunction) PyXRootD::FileSystem::RmDir,      METH_KEYWORDS, NULL },
      { "chmod",
          (PyCFunction) PyXRootD::FileSystem::ChMod,      METH_KEYWORDS, NULL },
      { "ping",
          (PyCFunction) PyXRootD::FileSystem::Ping,       METH_KEYWORDS, NULL },
      { "stat",
          (PyCFunction) PyXRootD::FileSystem::Stat,       METH_KEYWORDS, NULL },
      { "statvfs",
          (PyCFunction) PyXRootD::FileSystem::StatVFS,    METH_KEYWORDS, NULL },
      { "protocol",
          (PyCFunction) PyXRootD::FileSystem::Protocol,   METH_KEYWORDS, NULL },
      { "dirlist",
          (PyCFunction) PyXRootD::FileSystem::DirList,    METH_KEYWORDS, NULL },
      { "sendinfo",
          (PyCFunction) PyXRootD::FileSystem::SendInfo,   METH_KEYWORDS, NULL },
      { "prepare",
          (PyCFunction) PyXRootD::FileSystem::Prepare,    METH_KEYWORDS, NULL },
      { "get_property",
          (PyCFunction) PyXRootD::FileSystem::GetProperty, METH_KEYWORDS, NULL },
      { "set_property",
          (PyCFunction) PyXRootD::FileSystem::SetProperty, METH_KEYWORDS, NULL },
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int FileSystem_init( FileSystem *self, PyObject *args )
  {
    self->url = (URL *) PyObject_CallObject( (PyObject*) &URLType, args );

    if ( !self->url )
      return -1;

    self->filesystem = new XrdCl::FileSystem( *self->url->url );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void FileSystem_dealloc( FileSystem *self )
  {
    delete self->filesystem;
    Py_XDECREF( self->url );
    Py_TYPE(self)->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef FileSystemMembers[] =
  {
    { const_cast<char *>("url"), T_OBJECT_EX, offsetof(FileSystem, url), 0,
      const_cast<char *>("Server URL") },
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! FileSystem binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject FileSystemType =
    { PyVarObject_HEAD_INIT(NULL, 0)
    "pyxrootd.FileSystem",                      /* tp_name */
    sizeof(FileSystem),                         /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) FileSystem_dealloc,            /* tp_dealloc */
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
    filesystem_type_doc,                        /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    FileSystemMethods,                          /* tp_methods */
    FileSystemMembers,                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) FileSystem_init,                 /* tp_init */
  };
}

#endif /* PYXROOTD_FILESYSTEM_HH_ */

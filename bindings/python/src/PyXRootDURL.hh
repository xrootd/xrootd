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

#ifndef PYXROOTD_URL_HH_
#define PYXROOTD_URL_HH_

#include "PyXRootD.hh"

#include "XrdCl/XrdClURL.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! XrdCl::URL binding class
  //----------------------------------------------------------------------------
  class URL
  {
    public:
      static PyObject* IsValid( URL *self );
      static PyObject* GetHostId( URL *self, void *closure );
      static PyObject* GetProtocol( URL *self, void *closure );
      static int SetProtocol( URL *self, PyObject *protocol, void *closure );
      static PyObject* GetUserName( URL *self, void *closure );
      static int SetUserName( URL *self, PyObject *username, void *closure );
      static PyObject* GetPassword( URL *self, void *closure );
      static int SetPassword( URL *self, PyObject *password, void *closure );
      static PyObject* GetHostName( URL *self, void *closure );
      static int SetHostName( URL *self, PyObject *hostname, void *closure );
      static PyObject* GetPort( URL *self, void *closure );
      static int SetPort( URL *self, PyObject *port, void *closure );
      static PyObject* GetPath( URL *self, void *closure );
      static int SetPath( URL *self, PyObject *path, void *closure );
      static PyObject* GetPathWithParams( URL *self, void *closure );
      static PyObject* Clear( URL *self );

    public:
      PyObject_HEAD
      XrdCl::URL *url;
  };

  PyDoc_STRVAR(url_type_doc, "URL object (internal)");

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int URL_init( URL *self, PyObject *args )
  {
    const char *url;

    if ( !PyArg_ParseTuple( args, "s", &url ) )
      return -1;

    self->url = new XrdCl::URL( url );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void URL_dealloc( URL *self )
  {
    delete self->url;
    Py_TYPE(self)->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! __str__() equivalent
  //----------------------------------------------------------------------------
  static PyObject* URL_str( URL *self )
  {
    return PyUnicode_FromString( self->url->GetURL().c_str() );
  }

  //----------------------------------------------------------------------------
  //! Visible member definitions
  //----------------------------------------------------------------------------
  static PyMemberDef URLMembers[] =
    {
      { NULL } /* Sentinel */
    };

  static PyGetSetDef URLGetSet[] =
    {
      { const_cast<char *>("hostid"),
        (getter) URL::GetHostId, NULL, NULL, NULL },
      { const_cast<char *>("protocol"),
        (getter) URL::GetProtocol, (setter) URL::SetProtocol, NULL, NULL },
      { const_cast<char *>("username"),
        (getter) URL::GetUserName, (setter) URL::SetUserName, NULL, NULL },
      { const_cast<char *>("password"),
        (getter) URL::GetPassword, (setter) URL::SetPassword, NULL, NULL },
      { const_cast<char *>("hostname"),
        (getter) URL::GetHostName, (setter) URL::SetHostName, NULL, NULL },
      { const_cast<char *>("port"),
        (getter) URL::GetPort,     (setter) URL::SetPort, NULL, NULL },
      { const_cast<char *>("path"),
        (getter) URL::GetPath,     (setter) URL::SetPath, NULL, NULL },
      { const_cast<char *>("path_with_params"),
        (getter) URL::GetPathWithParams, NULL, NULL, NULL },
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef URLMethods[] = {
    { "is_valid", (PyCFunction) URL::IsValid, METH_NOARGS, NULL },
    { "clear",    (PyCFunction) URL::Clear,   METH_NOARGS, NULL },
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! URL binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject URLType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pyxrootd.URL",                             /* tp_name */
    sizeof(URL),                                /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) URL_dealloc,                   /* tp_dealloc */
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
    (reprfunc) URL_str,                         /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    url_type_doc,                               /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    URLMethods,                                 /* tp_methods */
    URLMembers,                                 /* tp_members */
    URLGetSet,                                  /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc) URL_init,                        /* tp_init */
  };
}

#endif /* PYXROOTD_URL_HH_ */

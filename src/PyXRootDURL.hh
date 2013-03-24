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

#ifndef URLTYPE_HH_
#define URLTYPE_HH_

#include "PyXRootD.hh"

#include "XrdCl/XrdClURL.hh"

  //----------------------------------------------------------------------------
  //! URL binding type definition
  //----------------------------------------------------------------------------
  typedef struct
  {
      PyObject_HEAD
      /* Type-specific fields */
      XrdCl::URL *url;
  } URL;

  //----------------------------------------------------------------------------
  //! Is the url valid
  //----------------------------------------------------------------------------
  static PyObject* IsValid( URL *self )
  {
    return Py_BuildValue( "O", PyBool_FromLong( self->url->IsValid() ) );
  }

  //----------------------------------------------------------------------------
  //! Get the host part of the URL (user:password\@host:port)
  //----------------------------------------------------------------------------
  static PyObject* GetHostId( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetHostId().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Get the protocol
  //----------------------------------------------------------------------------
  static PyObject* GetProtocol( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetProtocol().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set protocol
  //----------------------------------------------------------------------------
  static int SetProtocol( URL *self, PyObject *protocol, void *closure )
  {
    if ( !PyString_Check( protocol ) ) {
      PyErr_SetString( PyExc_TypeError, "protocol must be string" );
      return -1;
    }

    self->url->SetProtocol( std::string ( PyString_AsString( protocol ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the username
  //----------------------------------------------------------------------------
  static PyObject* GetUserName( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetUserName().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the username
  //----------------------------------------------------------------------------
  static int SetUserName( URL *self, PyObject *username, void *closure )
  {
    if ( !PyString_Check( username ) ) {
      PyErr_SetString( PyExc_TypeError, "username must be string" );
      return -1;
    }

    self->url->SetUserName( std::string( PyString_AsString( username ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the password
  //----------------------------------------------------------------------------
  static PyObject* GetPassword( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetPassword().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the password
  //----------------------------------------------------------------------------
  static int SetPassword( URL *self, PyObject *password, void *closure )
  {
    if ( !PyString_Check( password ) ) {
      PyErr_SetString( PyExc_TypeError, "password must be string" );
      return -1;
    }

    self->url->SetPassword( std::string( PyString_AsString( password ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the name of the target host
  //----------------------------------------------------------------------------
  static PyObject* GetHostName( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetHostName().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the host name
  //----------------------------------------------------------------------------
  static int SetHostName( URL *self, PyObject *hostname, void *closure )
  {
    if ( !PyString_Check( hostname ) ) {
      PyErr_SetString( PyExc_TypeError, "hostname must be string" );
      return -1;
    }

    self->url->SetHostName( std::string( PyString_AsString( hostname ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the target port
  //----------------------------------------------------------------------------
  static PyObject* GetPort( URL *self, void *closure )
  {
    return Py_BuildValue( "i", self->url->GetPort() );
  }

  //----------------------------------------------------------------------------
  //! Is the url valid
  //----------------------------------------------------------------------------
  static int SetPort( URL *self, PyObject *port, void *closure )
  {
    if ( !PyInt_Check( port ) ) {
      PyErr_SetString( PyExc_TypeError, "port must be int" );
      return -1;
    }

    self->url->SetPort( PyInt_AsLong( port ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the path
  //----------------------------------------------------------------------------
  static PyObject* GetPath( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetPath().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the path
  //----------------------------------------------------------------------------
  static int SetPath( URL *self, PyObject *path, void *closure )
  {
    if ( !PyString_Check( path ) ) {
      PyErr_SetString( PyExc_TypeError, "path must be string" );
      return -1;
    }

    self->url->SetPath( std::string( PyString_AsString( path ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the path with params
  //----------------------------------------------------------------------------
  static PyObject* GetPathWithParams( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetPathWithParams().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Clear the url
  //----------------------------------------------------------------------------
  static PyObject* Clear( URL *self )
  {
    self->url->Clear();
    Py_RETURN_NONE ;
  }

  //----------------------------------------------------------------------------
  //! Deallocation function, called when object is deleted
  //----------------------------------------------------------------------------
  static void URL_dealloc( URL *self )
  {
    //delete self->url;
    self->ob_type->tp_free( (PyObject*) self );
  }

  //----------------------------------------------------------------------------
  //! __init__() equivalent
  //----------------------------------------------------------------------------
  static int URL_init( URL *self, PyObject *args )
  {
    PyObject *url;

    if ( !PyArg_ParseTuple( args, "O", &url ) )
      return -1;

    self->url = (XrdCl::URL*) PyCObject_AsVoidPtr( url );
    return 0;
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
      { "hostid", (getter) GetHostId, NULL,
        "the host part of the URL (user:password@host:port)", NULL },
      { "protocol", (getter) GetProtocol, (setter) SetProtocol,
        "the protocol", NULL },
      { "username", (getter) GetUserName, (setter) SetUserName,
        "the username", NULL },
      { "password", (getter) GetPassword, (setter) SetPassword,
        "the password", NULL },
      { "hostname", (getter) GetHostName, (setter) SetHostName,
        "the name of the target host", NULL },
      { "port", (getter) GetPort, (setter) SetPort,
        "the target port", NULL },
      { "path", (getter) GetPath, (setter) SetPath,
        "the path", NULL },
      { "path_with_params", (getter) GetPathWithParams, NULL,
        "the path with parameters", NULL },
      { NULL } /* Sentinel */
    };

  //----------------------------------------------------------------------------
  //! Visible method definitions
  //----------------------------------------------------------------------------
  static PyMethodDef URLMethods[] = {
    { "is_valid", (PyCFunction) IsValid, METH_NOARGS,
        "Return the validity of the URL" },
    { "clear", (PyCFunction) Clear, METH_NOARGS,
        "Clear the url" },
    { NULL } /* Sentinel */
  };

  //----------------------------------------------------------------------------
  //! URL binding type object
  //----------------------------------------------------------------------------
  static PyTypeObject URLType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
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
    "URL object",                               /* tp_doc */
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


#endif /* URLTYPE_HH_ */

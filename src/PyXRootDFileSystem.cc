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

#include "PyXRootD.hh"
#include "PyXRootDClient.hh"
#include "AsyncResponseHandler.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Locate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Locate( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "flags", "timeout", "callback", NULL };
    const  char *path;
    uint16_t     flags, timeout = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO", kwlist,
        &path, &flags, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Locate( path, flags, handler, timeout ) );
    }

    // Synchronous mode
    else {
      XrdCl::LocationInfo *response = 0;
      status = self->filesystem->Locate( path, flags, response, timeout );
      pyresponse = ConvertResponse<XrdCl::LocationInfo>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Locate a file, recursively locate all disk servers
  //----------------------------------------------------------------------------
  PyObject* FileSystem::DeepLocate( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "flags", "timeout", "callback", NULL };
    const  char *path;
    uint16_t     flags, timeout = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO", kwlist,
        &path, &flags, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->DeepLocate( path, flags, handler, timeout ) );
    }

    // Synchronous mode
    else {
      XrdCl::LocationInfo *response = 0;
      status = self->filesystem->DeepLocate( path, flags, response, timeout );
      pyresponse = ConvertResponse<XrdCl::LocationInfo>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Move a directory or a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Mv( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "source", "dest", "timeout", "callback", NULL };
    const  char *source;
    const  char *dest;
    uint16_t     timeout = 5;
    PyObject    *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HO", kwlist,
        &source, &dest, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Mv( source, dest, handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->filesystem->Mv( source, dest, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Obtain server information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Query( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Truncate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Truncate( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Remove a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Rm( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Create a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::MkDir( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Remove a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::RmDir( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Change access mode on a directory or a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::ChMod( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Ping( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "timeout", "callback", NULL };
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO", kwlist,
        &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Ping( handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->filesystem->Ping( timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for a path
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Stat( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char *path;
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO", kwlist,
        &path, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Stat( path, handler, timeout ) );
    }

    // Synchronous mode
    else {
      XrdCl::StatInfo *response = 0;
      status = self->filesystem->Stat( path, response, timeout );
      pyresponse = ConvertResponse<XrdCl::StatInfo>(response);
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for a Virtual File System
  //----------------------------------------------------------------------------
  PyObject* FileSystem::StatVFS( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Obtain server protocol information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Protocol( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! List entries of a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::DirList( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Send info to the server (up to 1024 characters)
  //----------------------------------------------------------------------------
  PyObject* FileSystem::SendInfo( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Prepare one or more files for access
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Prepare( Client *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }
}

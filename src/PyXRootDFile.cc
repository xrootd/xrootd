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
#include "PyXRootDFile.hh"
#include "AsyncResponseHandler.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFile.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Open the file pointed to by the given URL
  //----------------------------------------------------------------------------
  PyObject* File::Open( File *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "url", "flags", "mode", "timeout", "callback",
                              NULL };
    const  char *url;
    uint16_t     flags    = 0, mode = 0, timeout = 5;
    PyObject    *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HHHO", kwlist,
        &url, &flags, &mode, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Open( url, flags, mode, handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->file->Open( url, flags, mode, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Close the file
  //----------------------------------------------------------------------------
  PyObject* File::Close( File *self, PyObject *args, PyObject *kwds )
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
      async( status = self->file->Close( handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->file->Close( timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for this file
  //----------------------------------------------------------------------------
  PyObject* File::Stat( File *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "force", "timeout", "callback", NULL };
    bool         force    = false;
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) {
      PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
      return NULL;
    }

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|iHO", kwlist,
        &force, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Stat( force, handler, timeout ) );
    }

    // Synchronous mode
    else {
      XrdCl::StatInfo *response = 0;
      status = self->file->Stat( force, response, timeout );
      pyresponse = ConvertResponse<XrdCl::StatInfo>(response);
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Read a data chunk at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Read( File *self, PyObject *args, PyObject *kwds )
  {
    static char   *kwlist[] = { "offset", "size", "timeout", "callback", NULL };
    uint64_t       offset   = 0;
    uint32_t       size     = 0;
    uint16_t       timeout  = 5;
    PyObject      *callback = NULL, *pyresponse = NULL;
    XrdCl::Buffer *buffer;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) {
      PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
      return NULL;
    }

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kIHO", kwlist,
        &offset, &size, &timeout, &callback ) ) return NULL;

    if (!size) {
      XrdCl::StatInfo *info = 0;
      XrdCl::XRootDStatus status = self->file->Stat(true, info, timeout);
      size = info->GetSize();
      if (info) delete info;
    }

    buffer = new XrdCl::Buffer( size );
    buffer->Zero();

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Read( offset, size, buffer->GetBuffer(),
                                        handler, timeout ) );
    }

    // Synchronous mode
    else {
      uint32_t bytesRead;
      status = self->file->Read( offset, size, buffer->GetBuffer(), bytesRead,
                                 timeout );
      pyresponse = ConvertResponse<XrdCl::Buffer>(buffer);
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Read a data chunk at a given offset, until the first newline encountered
  //----------------------------------------------------------------------------
  PyObject* File::Readline( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString( PyExc_NotImplementedError, "Method not implemented" );
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Read a data chunk at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Readlines( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString( PyExc_NotImplementedError, "Method not implemented" );
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Write a data chunk at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Write( File *self, PyObject *args, PyObject *kwds )
  {
    static char   *kwlist[] = { "buffer", "offset", "size", "timeout",
                                "callback", NULL };
    const  char   *buffer;
    uint64_t       offset   = 0;
    uint32_t       size     = 0;
    uint16_t       timeout  = 5;
    PyObject      *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) {
      PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
      return NULL;
    }

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|kIHO", kwlist,
        &buffer, &offset, &size, &timeout, &callback ) ) return NULL;

    if (!size) {
      size = strlen(buffer);
    }

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Write( offset, size, buffer, handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->file->Write( offset, size, buffer, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Commit all pending disk writes
  //----------------------------------------------------------------------------
  PyObject* File::Sync( File *self, PyObject *args, PyObject *kwds )
  {
    static char   *kwlist[] = { "timeout", "callback", NULL };
    uint16_t       timeout  = 5;
    PyObject      *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) {
      PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
      return NULL;
    }

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO", kwlist,
        &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Sync( handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->file->Sync( timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Truncate the file to a particular size
  //----------------------------------------------------------------------------
  PyObject* File::Truncate( File *self, PyObject *args, PyObject *kwds )
  {
    static char   *kwlist[] = { "size", "timeout", "callback", NULL };
    uint64_t       size     = 0;
    uint16_t       timeout  = 5;
    PyObject      *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) {
      PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
      return NULL;
    }

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "k|HO", kwlist,
        &size, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Truncate( size, handler, timeout ) );
    }

    // Synchronous mode
    else {
      status = self->file->Truncate( size, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Read scattered data chunks in one operation
  //----------------------------------------------------------------------------
  PyObject* File::VectorRead( File *self, PyObject *args, PyObject *kwds )
  {
    if ( !self->file->IsOpen() ) {
      PyErr_SetString( PyExc_ValueError, "I/O operation on closed file" );
      return NULL;
    }

    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Check if the file is open
  //----------------------------------------------------------------------------
  PyObject* File::IsOpen( File *self, PyObject *args, PyObject *kwds )
  {
    if ( !PyArg_ParseTuple( args, "" ) ) return NULL; // Allow no arguments
    return PyBool_FromLong(self->file->IsOpen());
  }

  //----------------------------------------------------------------------------
  //! Enable/disable state recovery procedures while the file is open for
  //! reading
  //----------------------------------------------------------------------------
  PyObject* File::EnableReadRecovery( File *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "enable", NULL };
    bool        enable   = NULL;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "i", kwlist, &enable ) )
      return NULL;

    self->file->EnableReadRecovery(enable);
    Py_RETURN_NONE;
  }

  //----------------------------------------------------------------------------
  //! Enable/disable state recovery procedures while the file is open for
  //! writing or read/write
  //----------------------------------------------------------------------------
  PyObject* File::EnableWriteRecovery( File *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "enable", NULL };
    bool        enable   = NULL;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "i", kwlist, &enable ) )
      return NULL;

    self->file->EnableWriteRecovery(enable);
    Py_RETURN_NONE;
  }

  //----------------------------------------------------------------------------
  //! Get the data server the file is accessed at
  //----------------------------------------------------------------------------
  PyObject* File::GetDataServer( File *self, PyObject *args, PyObject *kwds )
  {
    if ( !PyArg_ParseTuple( args, "" ) ) return NULL; // Allow no arguments
    return Py_BuildValue("s", self->file->GetDataServer().c_str());
  }
}

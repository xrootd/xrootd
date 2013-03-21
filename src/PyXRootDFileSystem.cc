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
    const  char            *path;
    XrdCl::OpenFlags::Flags flags    = XrdCl::OpenFlags::None;
    uint16_t                timeout  = 5;
    PyObject               *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:locate", kwlist,
        &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Locate( path, flags, handler, timeout ) );
    }

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
    const  char            *path;
    XrdCl::OpenFlags::Flags flags    = XrdCl::OpenFlags::None;
    uint16_t                timeout  = 5;
    PyObject               *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:deeplocate", kwlist,
        &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->DeepLocate( path, flags, handler, timeout ) );
    }

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

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HO:mv", kwlist,
        &source, &dest, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Mv( source, dest, handler, timeout ) );
    }

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
    static char *kwlist[] = { "querycode", "arg", "timeout", "callback", NULL };
    const  char *arg;
    uint16_t     timeout = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::QueryCode::Code queryCode;
    XrdCl::XRootDStatus    status;
    XrdCl::Buffer          argbuffer;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "is|HO:query", kwlist,
        &queryCode, &arg, &timeout, &callback ) ) return NULL;

    argbuffer.FromString(arg);

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Query( queryCode, argbuffer, handler, timeout ) );
    }

    else {
      XrdCl::Buffer *response;
      status = self->filesystem->Query( queryCode, argbuffer, response, timeout );
      pyresponse = ConvertResponse<XrdCl::Buffer>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Truncate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Truncate( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "size", "timeout", "callback", NULL };
    const  char *path;
    uint64_t     size     = 0;
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sk|HO:truncate", kwlist,
        &path, &size, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Truncate( path, size, handler, timeout ) );
    }

    else {
      status = self->filesystem->Truncate( path, size, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Remove a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Rm( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char *path;
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:rm", kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Rm( path, handler, timeout ) );
    }

    else {
      status = self->filesystem->Rm( path, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Create a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::MkDir( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "flags", "mode", "timeout", "callback",
                              NULL };
    const  char             *path;
    XrdCl::MkDirFlags::Flags flags    = XrdCl::MkDirFlags::None;
    XrdCl::Access::Mode      mode     = XrdCl::Access::None;
    uint16_t                 timeout  = 5;
    PyObject                *callback = NULL;
    XrdCl::XRootDStatus      status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|bkHO:mkdir", kwlist,
        &path, &flags, &mode, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->MkDir( path, flags, mode, handler, timeout ) );
    }

    else {
      status = self->filesystem->MkDir( path, flags, mode, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Remove a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::RmDir( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "timeout", "callback",
                              NULL };
    const  char *path;
    uint16_t     timeout = 5;
    PyObject    *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:rmdir", kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->RmDir( path, handler, timeout ) );
    }

    else {
      status = self->filesystem->RmDir( path, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
  }

  //----------------------------------------------------------------------------
  //! Change access mode on a directory or a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::ChMod( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "mode", "timeout", "callback",
                              NULL };
    const  char        *path;
    XrdCl::Access::Mode mode = XrdCl::Access::None;
    uint16_t            timeout = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:chmod", kwlist,
        &path, &mode, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->ChMod( path, mode, handler, timeout ) );
    }

    else {
      status = self->filesystem->ChMod( path, mode, timeout );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue("") );
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

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:ping", kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Ping( handler, timeout ) );
    }

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

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:stat", kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Stat( path, handler, timeout ) );
    }

    else {
      XrdCl::StatInfo *response = 0;
      status = self->filesystem->Stat( path, response, timeout );
      pyresponse = ConvertResponse<XrdCl::StatInfo>( response );
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
    static char *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char *path;
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:statvfs", kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfoVFS>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->StatVFS( path, handler, timeout ) );
    }

    else {
      XrdCl::StatInfoVFS *response = 0;
      status = self->filesystem->StatVFS( path, response, timeout );
      pyresponse = ConvertResponse<XrdCl::StatInfoVFS>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Obtain server protocol information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Protocol( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "timeout", "callback", NULL };
    uint16_t     timeout  = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:protocol", kwlist,
         &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::ProtocolInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Protocol( handler, timeout ) );
    }

    else {
      XrdCl::ProtocolInfo *response = 0;
      status = self->filesystem->Protocol( response, timeout );
      pyresponse = ConvertResponse<XrdCl::ProtocolInfo>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! List entries of a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::DirList( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "path", "flags", "timeout", "callback", NULL };
    const  char               *path;
    XrdCl::DirListFlags::Flags flags = XrdCl::DirListFlags::None;
    uint16_t                   timeout = 5;
    PyObject                  *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus        status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|bHO:dirlist", kwlist,
        &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::DirectoryList>( callback );
      if ( !handler ) return NULL;
      // TODO: find out why DirListFlags cannot be passed asynchronously
      async( status = self->filesystem->DirList( path, handler, timeout ) );
    }

    else {
      XrdCl::DirectoryList *list;
      status = self->filesystem->DirList( path, flags, list, timeout );
      pyresponse = ConvertType<XrdCl::DirectoryList>( list );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Send info to the server (up to 1024 characters)
  //----------------------------------------------------------------------------
  PyObject* FileSystem::SendInfo( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "info", "timeout", "callback", NULL };
    const  char *info;
    uint16_t     timeout = 5;
    PyObject    *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:sendinfo", kwlist,
        &info, &timeout, &callback ) ) return NULL;

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->SendInfo( info, handler, timeout ) );
    }

    else {
      XrdCl::Buffer *response;
      status = self->filesystem->SendInfo( info, response, timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }

  //----------------------------------------------------------------------------
  //! Prepare one or more files for access
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Prepare( Client *self, PyObject *args, PyObject *kwds )
  {
    static char *kwlist[] = { "files", "flags", "priority", "timeout",
                              "callback", NULL };
    XrdCl::PrepareFlags::Flags flags;
    uint8_t                    priority = 0;
    uint16_t                   timeout  = 5;
    PyObject                  *pyfiles, *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "Ob|bHO:prepare", kwlist,
        &pyfiles, &flags, &priority, &timeout, &callback ) ) return NULL;

    if ( !PyList_Check( pyfiles ) ) {
      PyErr_SetString( PyExc_TypeError, "files parameter must be a list" );
      return NULL;
    }

    std::vector<std::string> files;
    const char              *file;
    PyObject                *pyfile;

    // Convert python list to stl vector
    for ( int i = 0; i < PyList_Size( pyfiles ); ++i ) {
      pyfile = PyList_GetItem( pyfiles, i );
      if ( !pyfile ) return NULL;
      file = PyString_AsString( pyfile );
      files.push_back( std::string( file ) );
    }

    if ( callback ) { // async
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Prepare( files, flags, priority, handler, timeout ) );
    }

    else {
      XrdCl::Buffer *response;
      status = self->filesystem->Prepare( files, flags, priority, response, timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    if ( !pystatus ) return NULL;
    return (callback) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
  }
}

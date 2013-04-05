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
#include "PyXRootDFileSystem.hh"
#include "AsyncResponseHandler.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Locate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Locate( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char      *kwlist[] = { "path", "flags", "timeout", "callback",
                                         NULL };
    const  char            *path;
    XrdCl::OpenFlags::Flags flags    = XrdCl::OpenFlags::None;
    uint16_t                timeout  = 5;
    PyObject               *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:locate",
         (char**) kwlist, &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Locate( path, flags, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::LocationInfo *response = 0;
      status = self->filesystem->Locate( path, flags, response, timeout );
      pyresponse = ConvertType<XrdCl::LocationInfo>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Locate a file, recursively locate all disk servers
  //----------------------------------------------------------------------------
  PyObject* FileSystem::DeepLocate( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char      *kwlist[] = { "path", "flags", "timeout", "callback",
                                         NULL };
    const  char            *path;
    XrdCl::OpenFlags::Flags flags    = XrdCl::OpenFlags::None;
    uint16_t                timeout  = 5;
    PyObject               *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:deeplocate",
         (char**) kwlist, &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->DeepLocate( path, flags, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::LocationInfo *response = 0;
      status = self->filesystem->DeepLocate( path, flags, response, timeout );
      pyresponse = ConvertType<XrdCl::LocationInfo>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Move a directory or a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Mv( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "source", "dest", "timeout", "callback",
                                     NULL };
    const  char        *source;
    const  char        *dest;
    uint16_t            timeout = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HO:mv", (char**) kwlist,
        &source, &dest, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Mv( source, dest, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->Mv( source, dest, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Obtain server information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Query( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char     *kwlist[] = { "querycode", "arg", "timeout",
                                        "callback", NULL };
    const  char           *arg;
    uint16_t               timeout = 5;
    PyObject              *callback = NULL, *pyresponse = NULL;
    XrdCl::QueryCode::Code queryCode;
    XrdCl::XRootDStatus    status;
    XrdCl::Buffer          argbuffer;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "is|HO:query",
         (char**) kwlist, &queryCode, &arg, &timeout, &callback ) ) return NULL;

    argbuffer.FromString(arg);

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Query( queryCode, argbuffer, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::Buffer *response;
      status = self->filesystem->Query( queryCode, argbuffer, response, timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Truncate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Truncate( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "size", "timeout", "callback", NULL };
    const  char        *path;
    uint64_t            size     = 0;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sk|HO:truncate",
         (char**) kwlist, &path, &size, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Truncate( path, size, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->Truncate( path, size, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Remove a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Rm( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:rm", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Rm( path, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->Rm( path, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Create a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::MkDir( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char       *kwlist[] = { "path", "flags", "mode", "timeout",
                                          "callback", NULL };
    const  char             *path;
    XrdCl::MkDirFlags::Flags flags    = XrdCl::MkDirFlags::None;
    XrdCl::Access::Mode      mode     = XrdCl::Access::None;
    uint16_t                 timeout  = 5;
    PyObject                *callback = NULL;
    XrdCl::XRootDStatus      status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|bkHO:mkdir", (char**) kwlist,
        &path, &flags, &mode, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->MkDir( path, flags, mode, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->MkDir( path, flags, mode, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Remove a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::RmDir( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:rmdir", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->RmDir( path, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->RmDir( path, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Change access mode on a directory or a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::ChMod( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "mode", "timeout", "callback", NULL };
    const  char        *path;
    XrdCl::Access::Mode mode     = XrdCl::Access::None;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:chmod", (char**) kwlist,
        &path, &mode, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->ChMod( path, mode, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->ChMod( path, mode, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Ping( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:ping", (char**) kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Ping( handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->filesystem->Ping( timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for a path
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Stat( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:stat", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Stat( path, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::StatInfo *response = 0;
      status = self->filesystem->Stat( path, response, timeout );
      pyresponse = ConvertType<XrdCl::StatInfo>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for a Virtual File System
  //----------------------------------------------------------------------------
  PyObject* FileSystem::StatVFS( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:statvfs", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfoVFS>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->StatVFS( path, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::StatInfoVFS *response = 0;
      status = self->filesystem->StatVFS( path, response, timeout );
      pyresponse = ConvertType<XrdCl::StatInfoVFS>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Obtain server protocol information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Protocol( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:protocol", (char**) kwlist,
         &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::ProtocolInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Protocol( handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::ProtocolInfo *response = 0;
      status = self->filesystem->Protocol( response, timeout );
      pyresponse = ConvertType<XrdCl::ProtocolInfo>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! List entries of a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::DirList( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char         *kwlist[] = { "path", "flags", "timeout",
                                            "callback", NULL };
    const  char               *path;
    XrdCl::DirListFlags::Flags flags = XrdCl::DirListFlags::None;
    uint16_t                   timeout = 5;
    PyObject                  *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus        status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|bHO:dirlist",
         (char**) kwlist, &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::DirectoryList>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->DirList( path, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::DirectoryList *list;
      status = self->filesystem->DirList( path, flags, list, timeout );
      pyresponse = ConvertType<XrdCl::DirectoryList>( list );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Send info to the server (up to 1024 characters)
  //----------------------------------------------------------------------------
  PyObject* FileSystem::SendInfo( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "info", "timeout", "callback", NULL };
    const  char        *info;
    uint16_t            timeout = 5;
    PyObject           *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:sendinfo",
         (char**) kwlist, &info, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->SendInfo( info, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::Buffer *response = 0;
      status = self->filesystem->SendInfo( info, response, timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Prepare one or more files for access
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Prepare( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char         *kwlist[] = { "files", "flags", "priority",
                                            "timeout", "callback", NULL };
    XrdCl::PrepareFlags::Flags flags;
    uint8_t                    priority = 0;
    uint16_t                   timeout  = 5;
    PyObject                  *pyfiles, *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus        status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "Ob|bHO:prepare",
         (char**) kwlist, &pyfiles, &flags, &priority, &timeout, &callback ) )
      return NULL;

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

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Prepare( files, flags, priority,
                                                 handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::Buffer *response;
      status = self->filesystem->Prepare( files, flags, priority, response,
                                          timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }

    (void) FileSystemType; // Suppress unused variable warning
  }
}

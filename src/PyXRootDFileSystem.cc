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

#include "PyXRootD.hh"
#include "PyXRootDFileSystem.hh"
#include "PyXRootDCopyProcess.hh"
#include "AsyncResponseHandler.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Copy a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Copy( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char      *kwlist[] = { "source", "target", "force", NULL };
    const  char            *source, *target;
    bool                    force = false;
    PyObject               *pystatus = NULL;
    CopyProcess            *copyprocess = NULL;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|i:copy",
         (char**) kwlist, &source, &target, &force ) ) return NULL;

    CopyProcessType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &CopyProcessType ) < 0 ) return NULL;

    copyprocess = (CopyProcess*)
               PyObject_CallObject( (PyObject *) &CopyProcessType, NULL );
    if ( !copyprocess ) return NULL;

    copyprocess->AddJob( copyprocess, args, kwds );
    pystatus = copyprocess->Prepare( copyprocess, NULL, NULL );
    if ( !pystatus ) return NULL;
    if ( PyDict_GetItemString( pystatus, "ok" ) == Py_False ) return pystatus;

    pystatus = copyprocess->Run( copyprocess, PyTuple_New(0), PyDict_New() );
    if ( !pystatus ) return NULL;

    Py_DECREF( copyprocess );
    return pystatus;
  }

  //----------------------------------------------------------------------------
  //! Locate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Locate( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char      *kwlist[] = { "path", "flags", "timeout", "callback",
                                         NULL };
    const  char            *path;
    XrdCl::OpenFlags::Flags flags    = XrdCl::OpenFlags::None;
    uint16_t                timeout  = 0;
    PyObject               *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:locate",
         (char**) kwlist, &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Locate( path, flags, handler, timeout ) );
    }

    else {
      XrdCl::LocationInfo *response = 0;
      status = self->filesystem->Locate( path, flags, response, timeout );
      pyresponse = ConvertType<XrdCl::LocationInfo>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
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
    uint16_t                timeout  = 0;
    PyObject               *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:deeplocate",
         (char**) kwlist, &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::LocationInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->DeepLocate( path, flags, handler, timeout ) );
    }

    else {
      XrdCl::LocationInfo *response = 0;
      status = self->filesystem->DeepLocate( path, flags, response, timeout );
      pyresponse = ConvertType<XrdCl::LocationInfo>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
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
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HO:mv", (char**) kwlist,
        &source, &dest, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Mv( source, dest, handler, timeout ) );
    }

    else {
      status = self->filesystem->Mv( source, dest, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Obtain server information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Query( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char     *kwlist[] = { "querycode", "arg", "timeout",
                                        "callback", NULL };
    const  char           *arg;
    uint16_t               timeout  = 0;
    PyObject              *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
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
    }

    else {
      XrdCl::Buffer *response = 0;
      status = self->filesystem->Query( queryCode, argbuffer, response, timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Truncate a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Truncate( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "size", "timeout", "callback", NULL };
    const  char        *path;
    uint64_t            size     = 0;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sK|HO:truncate",
         (char**) kwlist, &path, &size, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Truncate( path, size, handler, timeout ) );
    }

    else {
      status = self->filesystem->Truncate( path, size, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Remove a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Rm( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:rm", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Rm( path, handler, timeout ) );
    }

    else {
      status = self->filesystem->Rm( path, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
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
    uint16_t                 timeout  = 0;
    PyObject                *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus      status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HHHO:mkdir", (char**) kwlist,
        &path, &flags, &mode, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->MkDir( path, flags, mode, handler, timeout ) );
    }

    else {
      status = self->filesystem->MkDir( path, flags, mode, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Remove a directory
  //----------------------------------------------------------------------------
  PyObject* FileSystem::RmDir( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:rmdir", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->RmDir( path, handler, timeout ) );
    }

    else {
      status = self->filesystem->RmDir( path, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Change access mode on a directory or a file
  //----------------------------------------------------------------------------
  PyObject* FileSystem::ChMod( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "mode", "timeout", "callback", NULL };
    const  char        *path;
    XrdCl::Access::Mode mode     = XrdCl::Access::None;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "sH|HO:chmod", (char**) kwlist,
        &path, &mode, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->ChMod( path, mode, handler, timeout ) );
    }

    else {
      status = self->filesystem->ChMod( path, mode, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Ping( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:ping", (char**) kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Ping( handler, timeout ) );
    }

    else {
      status = self->filesystem->Ping( timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for a path
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Stat( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:stat", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Stat( path, handler, timeout ) );
    }

    else {
      XrdCl::StatInfo *response = 0;
      status = self->filesystem->Stat( path, response, timeout );
      pyresponse = ConvertType<XrdCl::StatInfo>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for a Virtual File System
  //----------------------------------------------------------------------------
  PyObject* FileSystem::StatVFS( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "path", "timeout", "callback", NULL };
    const  char        *path;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:statvfs", (char**) kwlist,
        &path, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfoVFS>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->StatVFS( path, handler, timeout ) );
    }

    else {
      XrdCl::StatInfoVFS *response = 0;
      status = self->filesystem->StatVFS( path, response, timeout );
      pyresponse = ConvertType<XrdCl::StatInfoVFS>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Obtain server protocol information
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Protocol( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:protocol", (char**) kwlist,
         &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::ProtocolInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->Protocol( handler, timeout ) );
    }

    else {
      XrdCl::ProtocolInfo *response = 0;
      status = self->filesystem->Protocol( response, timeout );
      pyresponse = ConvertType<XrdCl::ProtocolInfo>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
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
    uint16_t                   timeout  = 0;
    PyObject                  *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus        status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|bHO:dirlist",
         (char**) kwlist, &path, &flags, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::DirectoryList>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->DirList( path, flags, handler, timeout ) );
    }

    else {
      XrdCl::DirectoryList *list = 0;
      status = self->filesystem->DirList( path, flags, list, timeout );
      pyresponse = ConvertType<XrdCl::DirectoryList>( list );
      delete list;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Send info to the server (up to 1024 characters)
  //----------------------------------------------------------------------------
  PyObject* FileSystem::SendInfo( FileSystem *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "info", "timeout", "callback", NULL };
    const  char        *info;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HO:sendinfo",
         (char**) kwlist, &info, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->filesystem->SendInfo( info, handler, timeout ) );
    }

    else {
      XrdCl::Buffer *response = 0;
      status = self->filesystem->SendInfo( info, response, timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
      delete response;
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
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
    uint16_t                   timeout  = 0;
    PyObject                  *pyfiles = NULL, *callback = NULL;
    PyObject                  *pyresponse = NULL, *pystatus = NULL;
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
    }

    else {
      XrdCl::Buffer *response = 0;
      status = self->filesystem->Prepare( files, flags, priority, response,
                                          timeout );
      pyresponse = ConvertType<XrdCl::Buffer>( response );
      delete response;
    }

    (void) FileSystemType; // Suppress unused variable warning

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Get property
  //----------------------------------------------------------------------------
  PyObject* FileSystem::GetProperty( FileSystem *self,
                                     PyObject   *args,
                                     PyObject   *kwds )
  {
    static const char *kwlist[] = { "name", NULL };
    char        *name = 0;
    std::string  value;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s:get_property",
         (char**) kwlist, &name ) ) return NULL;

    bool status = self->filesystem->GetProperty( name, value );
    return status ? Py_BuildValue( "s", value.c_str() ) : Py_None;
  }

  //----------------------------------------------------------------------------
  //! Set property
  //----------------------------------------------------------------------------
  PyObject* FileSystem::SetProperty( FileSystem *self,
                                     PyObject   *args,
                                     PyObject   *kwds )
  {
    static const char *kwlist[] = { "name", "value", NULL };
    char *name  = 0;
    char *value = 0;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss:set_property",
         (char**) kwlist, &name, &value ) ) return NULL;

    bool status = self->filesystem->SetProperty( name, value );
    return status ? Py_True : Py_False;
  }
}

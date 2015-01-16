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
#include "PyXRootDFile.hh"
#include "AsyncResponseHandler.hh"
#include "ChunkIterator.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Open the file pointed to by the given URL
  //----------------------------------------------------------------------------
  PyObject* File::Open( File *self, PyObject *args, PyObject *kwds )
  {
    static const char      *kwlist[] = { "url", "flags", "mode",
                                         "timeout", "callback", NULL };
    const  char            *url;
    XrdCl::OpenFlags::Flags flags    = XrdCl::OpenFlags::None;
    XrdCl::Access::Mode     mode     = XrdCl::Access::None;
    uint16_t                timeout  = 0;
    PyObject               *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HHHO:open",
         (char**) kwlist, &url, &flags, &mode, &timeout, &callback ) )
      return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Open( url, flags, mode, handler, timeout ) );
    }

    else {
      status = self->file->Open( url, flags, mode, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Close the file
  //----------------------------------------------------------------------------
  PyObject* File::Close( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:close", (char**) kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Close( handler, timeout ) );
    }

    else {
      status = self->file->Close( timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for this file
  //----------------------------------------------------------------------------
  PyObject* File::Stat( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "force", "timeout", "callback", NULL };
    bool                force    = false;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|iHO:stat", (char**) kwlist,
        &force, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      async( status = self->file->Stat( force, handler, timeout ) );
    }

    else {
      XrdCl::StatInfo *response = 0;
      status = self->file->Stat( force, response, timeout );
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
  //! Read a data chunk at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Read( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "offset", "size", "timeout", "callback",
                                      NULL };
    uint64_t            offset   = 0;
    uint32_t            size     = 0;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL, *pyresponse = NULL;
    char               *buffer   = 0;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|KIHO:read",
        (char**) kwlist, &offset, &size, &timeout, &callback ) ) return NULL;

    if (!size) {
      XrdCl::StatInfo *info = 0;
      XrdCl::XRootDStatus status = self->file->Stat(true, info, timeout);
      size = info->GetSize();
      if (info) delete info;
    }

    buffer = new char[size];

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::ChunkInfo>( callback );
      if ( !handler ) {
        delete[] buffer; return NULL;
      }
      async( status = self->file->Read( offset, size, buffer, handler, timeout ) );
    }

    else {
      uint32_t bytesRead;
      status = self->file->Read( offset, size, buffer, bytesRead, timeout );
      pyresponse = Py_BuildValue( "s#", buffer, bytesRead );
      delete[] buffer;
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
  //! Read a data chunk at a given offset, until the first newline encountered
  //----------------------------------------------------------------------------
  PyObject* File::ReadLine( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "offset", "size", "chunksize", NULL };
    uint32_t           offset    = 0;
    uint32_t           size      = 0;
    uint32_t           chunksize = 0;
    uint32_t    lastNewlineIndex = 0;
    bool        newlineFound     = false;
    PyObject          *pyline    = NULL;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kII:readline",
        (char**) kwlist, &offset, &size, &chunksize ) ) return NULL;

    //--------------------------------------------------------------------------
    // Default chunk size == 2MB
    //--------------------------------------------------------------------------
    if ( !chunksize ) chunksize = 1024 * 1024 * 2;

    //--------------------------------------------------------------------------
    // If we read multiple lines last time, return one here
    //--------------------------------------------------------------------------
    if ( !self->surplus->empty() )
    {
      XrdCl::Buffer *surplus = self->surplus->front();
      pyline = PyString_FromStringAndSize( surplus->GetBuffer(),
                                           surplus->GetSize() );
      self->surplus->pop_front();
      delete surplus;
      return pyline;
    }

    //--------------------------------------------------------------------------
    // Don't read more than "size"
    //--------------------------------------------------------------------------
    if ( size && self->currentOffset >= size )
    {
      self->chunk = new XrdCl::Buffer();
    }
    else
    {
      self->chunk = self->ReadChunk( self, chunksize, self->currentOffset );
      self->currentOffset += chunksize;
    }

    //--------------------------------------------------------------------------
    // We read nothing
    //--------------------------------------------------------------------------
    if ( self->chunk->GetSize() == 0 )
    {
      //delete self->chunk;

      //------------------------------------------------------------------------
      // We have no partial line, return empty string
      //------------------------------------------------------------------------
      if ( self->partial->GetSize() == 0 )
      {
        return PyString_FromString( "" );
      }
      //------------------------------------------------------------------------
      // We can return the partial line
      //------------------------------------------------------------------------
      else
      {
        pyline = PyString_FromStringAndSize( self->partial->GetBuffer(),
                                             self->partial->GetSize() );
        self->partial->Free();
        return pyline;
      }
    }

    //--------------------------------------------------------------------------
    // Keep reading chunks until we find one with newlines
    //--------------------------------------------------------------------------
    while ( !newlineFound )
    {
      for( uint32_t i = 0; i < self->chunk->GetSize(); ++i )
      {
        self->chunk->SetCursor( i );

        if( *self->chunk->GetBufferAtCursor() == '\n' )
        {
          //--------------------------------------------------------------------
          // We found a newline
          //--------------------------------------------------------------------
          newlineFound = true;

          //--------------------------------------------------------------------
          // This is the first line we've found so far
          //--------------------------------------------------------------------
          if ( !lastNewlineIndex && !pyline )
          {
            lastNewlineIndex = i;

            //------------------------------------------------------------------
            // Do we have a partial line to use up?
            //------------------------------------------------------------------
            if ( self->partial->GetSize() != 0 )
            {
              self->partial->Append( self->chunk->GetBuffer(),
                                     lastNewlineIndex + 1 );
              pyline = PyString_FromStringAndSize( self->partial->GetBuffer(),
                                                   self->partial->GetSize() );
              self->partial->Free();
            }

            //--------------------------------------------------------------------
            // No partial line
            //--------------------------------------------------------------------
            else
            {
              pyline = PyString_FromStringAndSize( self->chunk->GetBuffer(),
                                                   lastNewlineIndex + 1 );
            }
          }

          //--------------------------------------------------------------------
          // This is not the first line: append it to the surplus vector
          //--------------------------------------------------------------------
          else
          {
            XrdCl::Buffer *surplus = new XrdCl::Buffer();
            surplus->Append( self->chunk->GetBuffer( lastNewlineIndex + 1 ),
                             i - lastNewlineIndex );
            self->surplus->push_back( surplus );
            lastNewlineIndex = i;
          }
        }
      }

      //------------------------------------------------------------------------
      // We didn't find a newline in this chunk: read another
      //------------------------------------------------------------------------
      if ( !newlineFound ) {
        delete self->chunk;
        self->chunk = self->ReadChunk( self, chunksize, self->currentOffset );
        self->currentOffset += chunksize;
      }

      //------------------------------------------------------------------------
      // We have a partial line left in the buffer
      //------------------------------------------------------------------------
      if( lastNewlineIndex != self->chunk->GetSize() - 1 )
      {
        uint32_t off = 0, sze = 0;

        if( lastNewlineIndex == 0 )
        {
          if( *self->chunk->GetBuffer() == '\n' )
          {
            off = 1;
            sze = self->chunk->GetSize() - 1;
          }
          else
          {
            off = 0;
            sze = self->chunk->GetSize();
          }
        }
        else
        {
          off = lastNewlineIndex + 1;
          sze = self->chunk->GetSize() - lastNewlineIndex - 1;
        }

        self->partial->Append( self->chunk->GetBuffer( off ), sze );
      }
    }

    delete self->chunk;
    return pyline;
  }

  //----------------------------------------------------------------------------
  //! Read data chunks from a given offset, separated by newlines, until EOF
  //! encountered. Return list of lines read. A max read size can be specified,
  //! but it should be noted that using this method is probably a bad idea.
  //----------------------------------------------------------------------------
  PyObject* File::ReadLines( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "offset", "size", "chunksize", NULL };
    uint64_t           offset    = 0;
    uint32_t           size      = 0;
    uint32_t           chunksize = 0;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kII:readlines",
        (char**) kwlist, &offset, &size, &chunksize ) ) return NULL;

    PyObject *lines = PyList_New( 0 );
    PyObject *line  = NULL;

    for (;;)
    {
      line = self->ReadLine( self, args, kwds );
      if ( !line || PyString_Size( line ) == 0 ) {
        break;
      }
      PyList_Append( lines, line );
    }

    return lines;
  }

  //----------------------------------------------------------------------------
  //! Read a chunk of the given size from the given offset as a string
  //----------------------------------------------------------------------------
  XrdCl::Buffer* File::ReadChunk( File *self, uint64_t size, uint32_t offset )
  {
    XrdCl::XRootDStatus status;
    XrdCl::Buffer      *buffer;
    XrdCl::Buffer      *temp;
    uint32_t            bytesRead;

    temp = new XrdCl::Buffer( size );
    status = self->file->Read( offset, size, temp->GetBuffer(), bytesRead );

    buffer = new XrdCl::Buffer( bytesRead );
    buffer->Append( temp->GetBuffer(), bytesRead );
    delete temp;
    return buffer;
  }

  //----------------------------------------------------------------------------
  //! Read data chunks from a given offset of the given size, until EOF
  //! encountered. Return chunk iterator.
  //----------------------------------------------------------------------------
  PyObject* File::ReadChunks( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "offset", "chunksize", NULL };
    uint64_t           offset    = 0;
    uint32_t           chunksize = 1042 * 1024 * 2; // 2MB
    ChunkIterator     *iterator;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kI:readchunks",
         (char**) kwlist, &offset, &chunksize ) ) return NULL;

    ChunkIteratorType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &ChunkIteratorType ) < 0 ) return NULL;

    args = Py_BuildValue( "OOO", self, Py_BuildValue("k", offset),
                                       Py_BuildValue("I", chunksize) );
    iterator = (ChunkIterator*)
               PyObject_CallObject( (PyObject *) &ChunkIteratorType, args );
    Py_DECREF( args );
    if ( !iterator ) return NULL;

    return (PyObject *) iterator;
  }

  //----------------------------------------------------------------------------
  //! Write a data chunk at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Write( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "buffer", "offset", "size", "timeout",
                                     "callback", NULL };
    const  char        *buffer;
    int                 buffsize;
    uint64_t            offset   = 0;
    uint32_t            size     = 0;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s#|KIHO:write",
         (char**) kwlist, &buffer, &buffsize, &offset, &size, &timeout,
         &callback ) ) return NULL;

    if (!size) {
      size = buffsize;
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Write( offset, size, buffer, handler, timeout ) );
    }

    else {
      status = self->file->Write( offset, size, buffer, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Commit all pending disk writes
  //----------------------------------------------------------------------------
  PyObject* File::Sync( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:sync", (char**) kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Sync( handler, timeout ) );
    }

    else {
      status = self->file->Sync( timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Truncate the file to a particular size
  //----------------------------------------------------------------------------
  PyObject* File::Truncate( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "size", "timeout", "callback", NULL };
    uint64_t            size;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "K|HO:truncate",
         (char**) kwlist, &size, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Truncate( size, handler, timeout ) );
    }

    else {
      status = self->file->Truncate( size, timeout );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Read scattered data chunks in one operation
  //----------------------------------------------------------------------------
  PyObject* File::VectorRead( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "chunks", "timeout", "callback", NULL };
    uint16_t            timeout  = 0;
    uint64_t            offset   = 0;
    uint32_t            length   = 0;
    PyObject           *pychunks = NULL, *callback = NULL;
    PyObject           *pyresponse = NULL, *pystatus = NULL;
    XrdCl::XRootDStatus status;
    XrdCl::ChunkList    chunks;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "O|HO:vector_read",
         (char**) kwlist, &pychunks, &timeout, &callback ) ) return NULL;

    if ( !PyList_Check( pychunks ) ) {
      PyErr_SetString( PyExc_TypeError, "chunks parameter must be a list" );
      return NULL;
    }

    for ( int i = 0; i < PyList_Size( pychunks ); ++i ) {
      PyObject *chunk = PyList_GetItem( pychunks, i );

      if ( !PyTuple_Check( chunk ) || ( PyTuple_Size( chunk ) != 2 ) ) {
        PyErr_SetString( PyExc_TypeError, "vector_read() expects list of tuples"
                                          " of length 2" );
        return NULL;
      }

      // Check the offset/length values are valid
      uint64_t tmpoffset;
      uint32_t tmplength;
      if ( !PyArg_ParseTuple( chunk, "KI", &tmpoffset, &tmplength ) ) return NULL;

      if ( tmpoffset < 0 || tmplength < 0 ) {
        PyErr_SetString( PyExc_TypeError, "offsets and lengths must be positive" );
        return NULL;
      }

      offset = tmpoffset;
      length = tmplength;
      char    *buffer = new char[length];
      chunks.push_back( XrdCl::ChunkInfo( offset, length, buffer ) );
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler
        = GetHandler<XrdCl::VectorReadInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->VectorRead( chunks, 0, handler, timeout ) );
    }

    else {
      XrdCl::VectorReadInfo *info = 0;
      status = self->file->VectorRead( chunks, 0, info, timeout );
      pyresponse = ConvertType<XrdCl::VectorReadInfo>( info );
      delete info;
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
  // Perform a custom operation on an open file
  //----------------------------------------------------------------------------
  PyObject* File::Fcntl( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "arg", "timeout", "callback", NULL };
    const char         *buffer   = 0;
    int                 buffSize = 0;
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s#|HO:fcntl",
          (char**) kwlist, &buffer, &buffSize, &timeout, &callback ) )
      return NULL;

    XrdCl::Buffer arg; arg.Append( buffer, buffSize );

    if ( callback && callback != Py_None )
    {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if( !handler )
        return NULL;
      async( status = self->file->Fcntl( arg, handler, timeout ) );
    }

    else {
      XrdCl::Buffer *response = 0;
      status = self->file->Fcntl( arg, response, timeout );
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
  // Perform a custom operation on an open file
  //----------------------------------------------------------------------------
  PyObject* File::Visa( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:visa",
          (char**) kwlist, &timeout, &callback ) )
      return NULL;

    if ( callback && callback != Py_None )
    {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if( !handler )
        return NULL;
      async( status = self->file->Visa( handler, timeout ) );
    }

    else {
      XrdCl::Buffer *response = 0;
      status = self->file->Visa( response, timeout );
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
  //! Check if the file is open
  //----------------------------------------------------------------------------
  PyObject* File::IsOpen( File *self, PyObject *args, PyObject *kwds )
  {
    if ( !PyArg_ParseTuple( args, ":is_open" ) ) return NULL; // Allow no args
    return PyBool_FromLong(self->file->IsOpen());
  }

  //----------------------------------------------------------------------------
  //! Get property
  //----------------------------------------------------------------------------
  PyObject* File::GetProperty( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[] = { "name", NULL };
    char        *name = 0;
    std::string  value;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s:get_property",
         (char**) kwlist, &name ) ) return NULL;

    bool status = self->file->GetProperty( name, value );

    return status ? Py_BuildValue( "s", value.c_str() ) : Py_None;
  }

  //----------------------------------------------------------------------------
  //! Set property
  //----------------------------------------------------------------------------
  PyObject* File::SetProperty( File *self, PyObject *args, PyObject *kwds )
  {
    (void) FileType; // Suppress unused variable warning

    static const char *kwlist[] = { "name", "value", NULL };
    char *name  = 0;
    char *value = 0;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss:set_property",
         (char**) kwlist, &name, &value ) ) return NULL;

    bool status = self->file->SetProperty( name, value );
    return status ? Py_True : Py_False;
  }
}

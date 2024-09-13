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
    time_t                  timeout  = 0;
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
      async( status = self->file->Open( url, flags, mode, timeout ) );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "ON", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Close the file
  //----------------------------------------------------------------------------
  PyObject* File::Close( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    time_t              timeout  = 0;
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
      async( status = self->file->Close( timeout ) )
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "ON", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for this file
  //----------------------------------------------------------------------------
  PyObject* File::Stat( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "force", "timeout", "callback", NULL };
    int                 force    = 0;
    time_t              timeout  = 0;
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
      async( status = self->file->Stat( force, response, timeout ) );
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
    time_t              timeout  = 0;
    PyObject           *callback = NULL, *pystatus = NULL, *pyresponse = NULL;
    PyObject           *py_offset = NULL, *py_size = NULL, *py_timeout = NULL;
    char               *buffer   = 0;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|OOOO:read",
        (char**) kwlist, &py_offset, &py_size, &py_timeout, &callback ) ) return NULL;

    unsigned long long tmp_offset = 0;
    unsigned int tmp_size = 0;
    unsigned long long tmp_timeout = 0;

    if ( py_offset && PyObjToUllong( py_offset, &tmp_offset, "offset" ) )
      return NULL;

    if ( py_size && PyObjToUint(py_size, &tmp_size, "size" ) )
      return NULL;

    if ( py_timeout && PyObjToUllong(py_timeout, &tmp_timeout, "timeout" ) )
      return NULL;

    offset = (uint64_t)tmp_offset;
    size = (uint32_t)tmp_size;
    timeout = (time_t)tmp_timeout;

    if (!size) {
      XrdCl::StatInfo *info = 0;
      async( XrdCl::XRootDStatus status = self->file->Stat(true, info, timeout) );
      size = info->GetSize();
      if (info) delete info;
    }

    buffer = new char[size];

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::ChunkInfo>( callback );
      if ( !handler ) {
        delete[] buffer;
        return NULL;
      }
      async( status = self->file->Read( offset, size, buffer, handler, timeout ) );
    }

    else {
      uint32_t bytesRead = 0;
      async( status = self->file->Read( offset, size, buffer, bytesRead, timeout ) );
      pyresponse = PyBytes_FromStringAndSize( buffer, bytesRead );
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
  // Read a data chunk at a given offset, until the first newline encountered
  // or size data read.
  //----------------------------------------------------------------------------
  PyObject* File::ReadLine( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "offset", "size", "chunksize", NULL };
    uint64_t           offset    = 0;
    uint32_t           size      = 0;
    uint32_t           chunksize = 0;
    PyObject          *pyline    = NULL;
    PyObject          *py_offset = NULL, *py_size = NULL, *py_chunksize = NULL;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|OOO:readline",
        (char**) kwlist, &py_offset, &py_size, &py_chunksize ) ) return NULL;

    unsigned long long tmp_offset = 0;
    unsigned int tmp_size = 0, tmp_chunksize = 0;

    if ( py_offset && PyObjToUllong(py_offset, &tmp_offset, "offset" ) )
      return NULL;

    if ( py_size && PyObjToUint(py_size, &tmp_size, "size" ) )
      return NULL;

    if ( py_chunksize && PyObjToUint(py_chunksize, &tmp_chunksize, "chunksize" ) )
      return NULL;

    offset = (uint64_t)tmp_offset;
    size = (uint32_t)tmp_size;
    chunksize = (uint32_t)tmp_chunksize;
    uint64_t off_init = offset;

    if (offset == 0)
      offset = self->currentOffset;
    else
      self->currentOffset = offset;

    // Default chunk size is 2MB or equal to size if size less then 2MB
    if ( !chunksize ) chunksize = 1024 * 1024 * 2;
    if ( !size ) size = UINT_MAX;
    if ( size < chunksize ) chunksize = size;

    uint64_t off_end = offset + size;
    std::unique_ptr<XrdCl::Buffer> chunk;
    std::unique_ptr<XrdCl::Buffer> line = std::make_unique<XrdCl::Buffer>();

    while ( offset < off_end )
    {
      chunk.reset( self->ReadChunk( self, offset, chunksize ) );
      offset += chunk->GetSize();

      // Reached end of file
      if ( !chunk->GetSize() )
        break;

      // Check if we read a new line
      bool found_newline = false;

      for( uint32_t i = 0; i < chunk->GetSize(); ++i )
      {
        chunk->SetCursor( i );

        // Stop if found newline or read required amount of data
        if( ( *chunk->GetBufferAtCursor() == '\n') ||
            ( line->GetSize() + i >= size))
        {
          found_newline = true;
          line->Append( chunk->GetBuffer(), i + 1 );
          break;
        }
      }

      if ( !found_newline )
        line->Append( chunk->GetBuffer(), chunk->GetSize() );
      else
        break;
    }

    if ( line->GetSize() != 0 )
    {
      // Update file offset if default readline call
      if ( off_init == 0 )
        self->currentOffset += line->GetSize();

      pyline = PyUnicode_FromStringAndSize( line->GetBuffer(), line->GetSize() );
    }
    else
      pyline = PyUnicode_FromString( "" );

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
    PyObject *py_offset = NULL, *py_size = NULL, *py_chunksize = NULL;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kII:readlines",
          (char**) kwlist, &offset, &size, &chunksize ) ) return NULL;

    unsigned long long tmp_offset = 0;
    unsigned int tmp_size = 0, tmp_chunksize = 0;

    if ( py_offset && PyObjToUllong( py_offset, &tmp_offset, "offset" ) )
      return NULL;

    if ( py_size && PyObjToUint( py_size, &tmp_size, "size" ) )
      return NULL;

    if ( py_chunksize && PyObjToUint( py_chunksize, &tmp_chunksize, "chunksize" ) )
      return NULL;

    offset = (uint64_t)tmp_offset;
    size = (uint32_t)tmp_size;
    chunksize = (uint16_t)tmp_chunksize;

    PyObject *lines = PyList_New( 0 );
    PyObject *line  = NULL;

    for (;;)
    {
      line = self->ReadLine( self, args, kwds );

      if ( !line || PyUnicode_GET_LENGTH( line ) == 0 )
        break;

      PyList_Append( lines, line );
      Py_DECREF( line );
    }

    return lines;
  }

  //----------------------------------------------------------------------------
  //! Read a chunk of the given size from the given offset as a string
  //----------------------------------------------------------------------------
  XrdCl::Buffer* File::ReadChunk( File *self, uint64_t offset, uint32_t size )
  {
    XrdCl::XRootDStatus status;
    XrdCl::Buffer      *buffer;
    XrdCl::Buffer      *temp;
    uint32_t            bytesRead = 0;

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
    uint32_t           chunksize = 0;
    ChunkIterator     *iterator;
    PyObject          *py_offset = NULL, *py_chunksize = NULL;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|OO:readchunks",
         (char**) kwlist, &py_offset, &py_chunksize ) ) return NULL;

    unsigned long long tmp_offset = 0;
    unsigned int tmp_chunksize = 1024 * 1024 *2;  // 2 MB

    if ( py_offset && PyObjToUllong( py_offset, &tmp_offset, "offset" ) )
      return NULL;

    if ( py_chunksize && PyObjToUint( py_chunksize, &tmp_chunksize, "chunksize" ) )
      return NULL;

    offset = (uint64_t)tmp_offset;
    chunksize = (uint32_t)tmp_chunksize;
    ChunkIteratorType.tp_new = PyType_GenericNew;

    if ( PyType_Ready( &ChunkIteratorType ) < 0 ) return NULL;

    args = Py_BuildValue( "ONN", self, Py_BuildValue("k", offset),
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
    const  char *buffer;
    Py_ssize_t   buffsize;
    uint64_t     offset   = 0;
    uint32_t     size     = 0;
    time_t       timeout  = 0;
    PyObject    *callback = NULL, *pystatus = NULL;
    PyObject    *py_offset = NULL, *py_size = NULL, *py_timeout = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s#|OOOO:write",
         (char**) kwlist, &buffer, &buffsize, &py_offset, &py_size,
         &py_timeout, &callback ) ) return NULL;

    unsigned long long tmp_offset = 0;
    unsigned int tmp_size = 0;
    unsigned long long tmp_timeout = 0;

    if (py_offset && PyObjToUllong(py_offset, &tmp_offset, "offset"))
      return NULL;

    if (py_size && PyObjToUint(py_size, &tmp_size, "size"))
      return NULL;

    if (py_timeout && PyObjToUllong(py_timeout, &tmp_timeout, "timeout"))
      return NULL;

    offset = (uint64_t)tmp_offset;
    size = (uint32_t)tmp_size;
    timeout = (time_t)tmp_timeout;

    if (!size) {
      size = buffsize;
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Write( offset, size, buffer, handler, timeout ) );
    }

    else {
      async( status = self->file->Write( offset, size, buffer, timeout ) );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "ON", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Commit all pending disk writes
  //----------------------------------------------------------------------------
  PyObject* File::Sync( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    time_t              timeout  = 0;
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
      async( status = self->file->Sync( timeout ) );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "ON", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Truncate the file to a particular size
  //----------------------------------------------------------------------------
  PyObject* File::Truncate( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[] = { "size", "timeout", "callback", NULL };
    uint64_t           size;
    time_t             timeout  = 0;
    PyObject          *callback = NULL, *pystatus = NULL;
    PyObject          *py_size = NULL, *py_timeout = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "O|OO:truncate",
         (char**) kwlist, &py_size, &py_timeout, &callback ) ) return NULL;

    unsigned long long tmp_size = 0;
    unsigned long long tmp_timeout = 0;

    if ( py_size && PyObjToUllong( py_size, &tmp_size, "size" ) )
      return NULL;

    if ( py_timeout && PyObjToUllong( py_timeout, &tmp_timeout, "timeout" ) )
      return NULL;

    size = (uint64_t)tmp_size;
    timeout = (time_t)tmp_timeout;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Truncate( size, handler, timeout ) );
    }

    else {
      async( status = self->file->Truncate( size, timeout ) );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "ON", pystatus, Py_BuildValue( "" ) );
    Py_DECREF( pystatus );
    return o;
  }

  //----------------------------------------------------------------------------
  //! Read scattered data chunks in one operation
  //----------------------------------------------------------------------------
  PyObject* File::VectorRead( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "chunks", "timeout", "callback", NULL };
    time_t              timeout  = 0;
    uint64_t            offset   = 0;
    uint32_t            length   = 0;
    PyObject           *pychunks = NULL, *callback = NULL;
    PyObject           *pyresponse = NULL, *pystatus = NULL, *py_timeout = NULL;
    XrdCl::XRootDStatus status;
    XrdCl::ChunkList    chunks;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "O|OO:vector_read",
         (char**) kwlist, &pychunks, &py_timeout, &callback ) ) return NULL;

    unsigned long long tmp_timeout = 0;

    if ( py_timeout && PyObjToUllong( py_timeout, &tmp_timeout, "timeout" ) )
      return NULL;

    timeout = (time_t)tmp_timeout;

    if ( !PyList_Check( pychunks ) ) {
      PyErr_SetString( PyExc_TypeError, "chunks parameter must be a list" );
      return NULL;
    }

    struct chunkGuard {
      chunkGuard(XrdCl::ChunkList &c) : c_(&c) { }
      ~chunkGuard() {
        if (c_)
          std::for_each(c_->begin(), c_->end(), [](XrdCl::ChunkInfo &ci) { delete[] (char*)ci.buffer; });
      }
      void disarm() { c_ = nullptr; }

       XrdCl::ChunkList *c_;
    } cg(chunks);

    for ( int i = 0; i < PyList_Size( pychunks ); ++i ) {
      PyObject *chunk = PyList_GetItem( pychunks, i );

      if ( !PyTuple_Check( chunk ) || ( PyTuple_Size( chunk ) != 2 ) ) {
        PyErr_SetString( PyExc_TypeError, "vector_read() expects list of tuples"
                                          " of length 2" );
        return NULL;
      }

      // Check that offset and length values are valid
      unsigned long long tmp_offset = 0;
      unsigned int tmp_length = 0;

      if ( PyObjToUllong( PyTuple_GetItem( chunk, 0 ), &tmp_offset, "offset" ) )
        return NULL;

      if ( PyObjToUint( PyTuple_GetItem( chunk, 1 ), &tmp_length, "length" ) )
        return NULL;

      offset = (uint64_t)tmp_offset;
      length = (uint32_t)tmp_length;
      char    *buffer = new char[length];
      chunks.push_back( XrdCl::ChunkInfo( offset, length, buffer ) );
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler
          = GetHandler<XrdCl::VectorReadInfo>( callback );
      if ( !handler ) return NULL;
      cg.disarm(); // handler will call ConvertType and free chunk buffers
      async( status = self->file->VectorRead( chunks, 0, handler, timeout ) );
    }
    else {
      XrdCl::VectorReadInfo *info = 0;
      async( status = self->file->VectorRead( chunks, 0, info, timeout ) );
      cg.disarm(); // ConvertType will free chunk buffers
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
    Py_ssize_t          buffSize = 0;
    time_t              timeout  = 0;
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
      async( status = self->file->Fcntl( arg, response, timeout ) );
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
    time_t              timeout  = 0;
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
      async( status = self->file->Visa( response, timeout ) );
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

  //----------------------------------------------------------------------------
  //! Set Extended File Attributes
  //----------------------------------------------------------------------------
  PyObject* File::SetXAttr( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "attrs", "timeout", "callback", NULL };

    std::vector<XrdCl::xattr_t>  attrs;
    time_t timeout = 0;

    PyObject    *callback = NULL, *pystatus   = NULL;
    PyObject    *pyattrs  = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "O|HO:set_xattr",
         (char**) kwlist, &pyattrs, &timeout, &callback ) ) return NULL;

    // it should be a list
    if( !PyList_Check( pyattrs ) )
      return NULL;

    // now parse the input
    Py_ssize_t size = PyList_Size( pyattrs );
    attrs.reserve( size );
    for( ssize_t i = 0; i < size; ++i )
    {
      // get the item at respective index
      PyObject *item = PyList_GetItem( pyattrs, i );
      // make sure the item is a tuple
      if( !item || !PyTuple_Check( item ) )
        return NULL;
      // make sure the tuple size equals to 2
      if( PyTuple_Size( item ) != 2 )
        return NULL;
      // extract the attribute name from the tuple
      PyObject *py_name = PyTuple_GetItem( item, 0 );
      if( !PyUnicode_Check( py_name ) )
        return NULL;
      std::string name = PyUnicode_AsUTF8( py_name );
      // extract the attribute value from the tuple
      PyObject *py_value = PyTuple_GetItem( item, 1 );
      if( !PyUnicode_Check( py_value ) )
        return NULL;
      std::string value = PyUnicode_AsUTF8( py_value );
      // update the C++ list of xattrs
      attrs.push_back( XrdCl::xattr_t( name, value ) );
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<std::vector<XrdCl::XAttrStatus>>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->SetXAttr( attrs, handler, timeout ) );
    }

    else {
      std::vector<XrdCl::XAttrStatus>  result;
      async( status = self->file->SetXAttr( attrs, result, timeout ) );
      pyresponse = ConvertType( &result );
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
  //! Get Extended File Attributes
  //----------------------------------------------------------------------------
  PyObject* File::GetXAttr( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "attrs", "timeout", "callback", NULL };

    std::vector<std::string>  attrs;
    time_t timeout = 0;

    PyObject    *callback = NULL, *pystatus   = NULL;
    PyObject    *pyattrs  = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "O|HO:set_xattr",
         (char**) kwlist, &pyattrs, &timeout, &callback ) ) return NULL;

    // it should be a list
    if( !PyList_Check( pyattrs ) )
      return NULL;

    // now parse the input
    Py_ssize_t size = PyList_Size( pyattrs );
    attrs.reserve( size );
    for( ssize_t i = 0; i < size; ++i )
    {
      // get the item at respective index
      PyObject *item = PyList_GetItem( pyattrs, i );
      // make sure the item is a string
      if( !item || !PyUnicode_Check( item ) )
        return NULL;
      std::string name = PyUnicode_AsUTF8( item );
      // update the C++ list of xattrs
      attrs.push_back( name );
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<std::vector<XrdCl::XAttr>>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->GetXAttr( attrs, handler, timeout ) );
    }

    else {
      std::vector<XrdCl::XAttr>  result;
      async( status = self->file->GetXAttr( attrs, result, timeout ) );
      pyresponse = ConvertType( &result );
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
  //! Delete Extended File Attributes
  //----------------------------------------------------------------------------
  PyObject* File::DelXAttr( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "attrs", "timeout", "callback", NULL };

    std::vector<std::string>  attrs;
    time_t timeout = 0;

    PyObject    *callback = NULL, *pystatus   = NULL;
    PyObject    *pyattrs  = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "O|HO:set_xattr",
         (char**) kwlist, &pyattrs, &timeout, &callback ) ) return NULL;

    // it should be a list
    if( !PyList_Check( pyattrs ) )
      return NULL;

    // now parse the input
    Py_ssize_t size = PyList_Size( pyattrs );
    attrs.reserve( size );
    for( ssize_t i = 0; i < size; ++i )
    {
      // get the item at respective index
      PyObject *item = PyList_GetItem( pyattrs, i );
      // make sure the item is a string
      if( !item || !PyUnicode_Check( item ) )
        return NULL;
      std::string name = PyUnicode_AsUTF8( item );
      // update the C++ list of xattrs
      attrs.push_back( name );
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<std::vector<XrdCl::XAttrStatus>>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->DelXAttr( attrs, handler, timeout ) );
    }

    else {
      std::vector<XrdCl::XAttrStatus>  result;
      async( status = self->file->DelXAttr( attrs, result, timeout ) );
      pyresponse = ConvertType( &result );
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
  //! List Extended File Attributes
  //----------------------------------------------------------------------------
  PyObject* File::ListXAttr( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };

    time_t timeout = 0;

    PyObject    *callback   = NULL, *pystatus = NULL;
    PyObject    *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:set_xattr",
         (char**) kwlist, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<std::vector<XrdCl::XAttr>>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->ListXAttr( handler, timeout ) );
    }

    else {
      std::vector<XrdCl::XAttr>  result;
      async( status = self->file->ListXAttr( result, timeout ) );
      pyresponse = ConvertType( &result );
    }

    pystatus = ConvertType<XrdCl::XRootDStatus>( &status );
    PyObject *o = ( callback && callback != Py_None ) ?
            Py_BuildValue( "O", pystatus ) :
            Py_BuildValue( "OO", pystatus, pyresponse );
    Py_DECREF( pystatus );
    Py_XDECREF( pyresponse );
    return o;
  }
}

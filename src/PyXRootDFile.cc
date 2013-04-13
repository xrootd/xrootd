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

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kIHO:read",
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
    XrdCl::Buffer *chunk;
    uint64_t       chunksize = 1024 * 1024 * 2; // 2MB
    PyObject      *pyline    = NULL;

    // If we read multiple lines last time, return one here
    if ( !self->surplus->empty() )
    {
      pyline = PyString_FromStringAndSize( self->surplus->front()->GetBuffer(),
                                           self->surplus->front()->GetSize() );
      self->surplus->pop_front();
      return pyline;
    }

    chunk = self->ReadChunk( self, chunksize, self->currentOffset );
    self->currentOffset += chunksize;

    // We read nothing
    if ( chunk->GetSize() == 0 )
    {
      delete chunk;

      // We have no partial line, return empty string
      if ( self->partial->GetSize() == 0 )
      {
        return Py_BuildValue( "s", "" );
      }
      // We can return the partial line
      else
      {
        pyline = PyString_FromStringAndSize( self->partial->GetBuffer(),
                                             self->partial->GetSize() );
        self->partial->Free();
        return pyline;
      }
    }

    for( uint32_t i = 0; i < chunk->GetSize(); ++i )
    {
      chunk->SetCursor( i );

      if( chunk->GetBufferAtCursor() == "\n" )
      {
        // we found a newline... what now? (go to sleep)
      }
    }

//    XrdCl::ChunkInfo         *chunk;
//    std::string               line;
//    std::vector<std::string> *lines;
//    PyObject                 *pyline    = NULL;
//
//    // If we read multiple lines last time, return one here
//    if ( !self->surplus->empty() ) {
//      pyline = PyString_FromStringAndSize( self->surplus->front().c_str(),
//                                           self->surplus->front().length() );
//      self->surplus->pop_front();
//      return pyline;
//    }
//
//    // Read a 2MB chunk
//    chunk = ReadChunk( self, chunksize, self->currentOffset );
//
//    // We read nothing
//    if ( chunk->length == 0 ) {
//      delete chunk;
//
//      // We have no partial line, return empty string
//      if ( self->partial->empty() ) {
//        return Py_BuildValue( "s", "" );
//      }
//      // We can return the partial line
//      else {
//        pyline = PyString_FromStringAndSize( self->partial->c_str(),
//                                             self->partial->length() );
//        self->partial->clear();
//        return pyline;
//      }
//    }
//
//    self->currentOffset += chunksize;
//
//    while ( !HasNewline( (const char*) chunk->buffer, chunk->length )
//            && chunk->length == chunksize )
//    {
//      self->partial->append( (char*) chunk->buffer, chunk->length );
//      delete[] (char*) chunk->buffer; delete chunk;
//      chunk = ReadChunk( self, chunksize, self->currentOffset );
//      self->currentOffset += chunksize;
//    }
//
//    lines = SplitNewlines( (const char*) chunk->buffer, chunk->length );
//
//    if ( lines->size() == 2 ) {
//      self->partial = &lines->back();
//    }
//    else if ( lines->size() > 2 ) {
//      self->surplus->insert(
//      self->surplus->end(), lines->begin() + 1, lines->end() );
//    }
//    else {
//      line = *self->partial + lines->front();
//      self->partial->clear();
//    }
//
//    pyline = PyString_FromStringAndSize( line.c_str(), line.length() );
//    if ( !pyline ) return NULL;
//    delete[] (char*) chunk->buffer;
//    delete chunk;
//    delete lines;
//    return pyline;
  }

  //----------------------------------------------------------------------------
  //! Read data chunks from a given offset, separated by newlines, until EOF
  //! encountered. Return list of lines read. A max read size can be specified,
  //! but it should be noted that using this method is probably a bad idea.
  //----------------------------------------------------------------------------
  PyObject* File::ReadLines( File *self, PyObject *args, PyObject *kwds )
  {
    static const char        *kwlist[]  = { "offset", "size", NULL };
    uint64_t                  offset    = 0;
    uint32_t                  size      = 0;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kI:readlines",
        (char**) kwlist, &offset, &size ) ) return NULL;

    PyObject *lines = PyList_New( 0 );
    PyObject *line  = NULL;

    for (;;)
    {
      line = self->ReadLine( self, NULL, NULL );
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
    uint32_t            bytesRead;

    buffer = new XrdCl::Buffer( size );
    status = self->file->Read( offset, size, buffer->GetBuffer(), bytesRead );
    return buffer;
  }

  //----------------------------------------------------------------------------
  //! Read data chunks from a given offset of the given size, until EOF
  //! encountered. Return chunk iterator.
  //----------------------------------------------------------------------------
  PyObject* File::ReadChunks( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "offset", "blocksize", NULL };
    uint64_t           offset    = 0;
    uint32_t           blocksize = 1042 * 1024 * 2; // 2MB
    ChunkIterator     *iterator;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|Ik:readchunks",
         (char**) kwlist, &offset, &blocksize ) ) return NULL;

    ChunkIteratorType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &ChunkIteratorType ) < 0 ) return NULL;

    args = Py_BuildValue( "OkI", self, offset, blocksize );
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

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s#|kIHO:write",
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

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "k|HO:truncate",
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
      int tmpoffset, tmplength;
      if ( !PyArg_ParseTuple( chunk, "ii", &tmpoffset, &tmplength ) ) return NULL;

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
  //! Check if the file is open
  //----------------------------------------------------------------------------
  PyObject* File::IsOpen( File *self, PyObject *args, PyObject *kwds )
  {
    if ( !PyArg_ParseTuple( args, ":is_open" ) ) return NULL; // Allow no args
    return PyBool_FromLong(self->file->IsOpen());
  }

  //----------------------------------------------------------------------------
  //! Enable/disable state recovery procedures while the file is open for
  //! reading
  //----------------------------------------------------------------------------
  PyObject* File::EnableReadRecovery( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[] = { "enable", NULL };
    bool               enable   = NULL;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "i:enable_read_recovery",
         (char**) kwlist, &enable ) ) return NULL;

    self->file->EnableReadRecovery(enable);
    Py_RETURN_NONE;
  }

  //----------------------------------------------------------------------------
  //! Enable/disable state recovery procedures while the file is open for
  //! writing or read/write
  //----------------------------------------------------------------------------
  PyObject* File::EnableWriteRecovery( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[] = { "enable", NULL };
    bool               enable   = NULL;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "i:enable_write_recovery",
         (char**) kwlist, &enable ) ) return NULL;

    self->file->EnableWriteRecovery(enable);
    Py_RETURN_NONE;
  }

  //----------------------------------------------------------------------------
  //! Get the data server the file is accessed at
  //----------------------------------------------------------------------------
  PyObject* File::GetDataServer( File *self, PyObject *args, PyObject *kwds )
  {
    if ( !PyArg_ParseTuple( args, ":get_data_server" ) ) return NULL; // No args
    return Py_BuildValue("s", self->file->GetDataServer().c_str());

    (void) FileType; // Suppress unused variable warning
  }
}

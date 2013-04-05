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
    uint16_t                timeout  = 5;
    PyObject               *callback = NULL;
    XrdCl::XRootDStatus     status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HHHO:open",
         (char**) kwlist, &url, &flags, &mode, &timeout, &callback ) )
      return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Open( url, flags, mode, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->file->Open( url, flags, mode, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Close the file
  //----------------------------------------------------------------------------
  PyObject* File::Close( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:close", (char**) kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Close( handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->file->Close( timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for this file
  //----------------------------------------------------------------------------
  PyObject* File::Stat( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "force", "timeout", "callback", NULL };
    bool                force    = false;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL, *pyresponse = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|iHO:stat", (char**) kwlist,
        &force, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::StatInfo>( callback );
      async( status = self->file->Stat( force, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::StatInfo *response = 0;
      status = self->file->Stat( force, response, timeout );
      pyresponse = ConvertResponse<XrdCl::StatInfo>( response );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
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
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL, *pyresponse = NULL;
    XrdCl::Buffer      *buffer;
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

    buffer = new XrdCl::Buffer( size );
    buffer->Zero();

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::Buffer>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Read( offset, size, buffer->GetBuffer(),
                                        handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      uint32_t bytesRead;
      status = self->file->Read( offset, size, buffer->GetBuffer(), bytesRead,
                                 timeout );
      pyresponse = ConvertResponse<XrdCl::Buffer>(buffer);
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
  }

  //----------------------------------------------------------------------------
  //! Read a data chunk at a given offset, until the first newline encountered
  //----------------------------------------------------------------------------
  PyObject* File::ReadLine( File *self, PyObject *args, PyObject *kwds )
  {
    uint64_t    chunksize = 1024 * 1024 * 2; // 2MB
    std::string chunk;
    std::string line;
    PyObject   *pyline    = NULL;
    std::vector<std::string> *lines;

    if ( !self->surplus->empty() ) {
      pyline = PyString_FromString( self->surplus->front().c_str() );
      self->surplus->pop_front();
      return pyline;
    }

    chunk = ReadChunk( self, chunksize, self->currentOffset );
    if ( chunk.empty() ) {

      if ( self->partial->empty() ) {
        return Py_BuildValue( "s", "" );
      }
      else {
        pyline = PyString_FromString( self->partial->c_str() );
        self->partial->clear();
        return pyline;
      }
    }

    self->currentOffset += chunksize;

    while ( !HasNewline( chunk ) && chunk.size() == chunksize ) {
      self->partial->append( chunk );
      chunk = ReadChunk( self, chunksize , self->currentOffset );
      self->currentOffset += chunksize;
    }

    lines = SplitNewlines( chunk );

    if ( lines->size() == 0 ) {
      line = *self->partial;
    }
    else {
      line = *self->partial + lines->front();
    }

    if ( lines->size() == 2 ) {
      self->partial = &lines->back();
    }
    else if ( lines->size() > 2 ) {
      self->surplus->insert(
      self->surplus->end(), lines->begin() + 1, lines->end() );
    }
    else {
      self->partial->clear();
    }

    pyline = PyString_FromString( line.c_str() );
    if ( !pyline ) return NULL;
    return pyline;
  }

  //----------------------------------------------------------------------------
  //! Read data chunks from a given offset, separated by newlines, until EOF
  //! encountered. Return list of lines read. A max read size can be specified,
  //! but it should be noted that using this method is probably a bad idea.
  //----------------------------------------------------------------------------
  PyObject* File::ReadLines( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "offset", "size", NULL };
    uint64_t           offset    = 0;
    uint32_t           size      = 0;
    uint32_t           bytesRead = 0;
    XrdCl::Buffer     *buffer;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|kI:readlines",
        (char**) kwlist, &offset, &size ) ) return NULL;

    //--------------------------------------------------------------------------
    // Find the file size
    //--------------------------------------------------------------------------
    if (!size) {
      XrdCl::StatInfo    *info   = 0;
      XrdCl::XRootDStatus status = self->file->Stat( true, info );
      size = info->GetSize();
      if (info) delete info;
    }

    //--------------------------------------------------------------------------
    // Read the whole file...
    //--------------------------------------------------------------------------
    buffer = new XrdCl::Buffer( size + 1 );
    buffer->Zero();
    self->file->Read( offset, size, buffer->GetBuffer(), bytesRead );

    // Convert into list, split by newlines
    std::istringstream stream(
                       std::string( buffer->GetBuffer(), buffer->GetSize() ) );
    std::string        line;
    PyObject          *lines = PyList_New( 0 );

    while ( std::getline( stream, line )) {
      line += '\n'; // Restore the newline
      PyList_Append( lines, PyString_FromString( line.c_str() ) );
      if ( stream.eof() ) break;
    }

    return lines;
  }

  //----------------------------------------------------------------------------
  //! Read a chunk of the given size from the given offset as a string
  //----------------------------------------------------------------------------
  std::string File::ReadChunk( File *self, uint64_t chunksize, uint32_t offset )
  {
    PyObject *args = Py_BuildValue( "kI", offset, chunksize );
    if ( !args ) return NULL;

    PyObject *pychunk = PyTuple_GetItem( self->Read( self, args, NULL ), 1 );
    if ( !pychunk ) return NULL;

    return PyString_AsString( pychunk );
  }

  //----------------------------------------------------------------------------
  //! Read data chunks from a given offset of the given size, until EOF
  //! encountered. Return list of chunks read.
  //----------------------------------------------------------------------------
  PyObject* File::ReadChunks( File *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]  = { "blocksize", "offset", NULL };
    uint32_t           blocksize = 4096;
    uint64_t           offset    = 0;
    ChunkIterator     *iterator;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|Ik:readchunks",
         (char**) kwlist, &blocksize, &offset ) ) return NULL;

    ChunkIteratorType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &ChunkIteratorType ) < 0 ) return NULL;

    args = Py_BuildValue( "OIk", self, blocksize, offset );
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
    uint64_t            offset   = 0;
    uint32_t            size     = 0;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|kIHO:write",
         (char**) kwlist, &buffer, &offset, &size, &timeout, &callback ) )
      return NULL;

    if (!size) {
      size = strlen(buffer);
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Write( offset, size, buffer, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->file->Write( offset, size, buffer, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Commit all pending disk writes
  //----------------------------------------------------------------------------
  PyObject* File::Sync( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "timeout", "callback", NULL };
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|HO:sync", (char**) kwlist,
        &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Sync( handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->file->Sync( timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Truncate the file to a particular size
  //----------------------------------------------------------------------------
  PyObject* File::Truncate( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "size", "timeout", "callback", NULL };
    uint64_t            size;
    uint16_t            timeout  = 5;
    PyObject           *callback = NULL;
    XrdCl::XRootDStatus status;

    if ( !self->file->IsOpen() ) return FileClosedError();

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "k|HO:truncate",
         (char**) kwlist, &size, &timeout, &callback ) ) return NULL;

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->Truncate( size, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      status = self->file->Truncate( size, timeout );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  Py_BuildValue("") );
    }
  }

  //----------------------------------------------------------------------------
  //! Read scattered data chunks in one operation
  //----------------------------------------------------------------------------
  PyObject* File::VectorRead( File *self, PyObject *args, PyObject *kwds )
  {
    static const char  *kwlist[] = { "chunks", "timeout", "callback", NULL };
    uint16_t            timeout  = 5;
    PyObject           *pychunks = NULL, *callback = NULL, *pyresponse = NULL;
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

      if ( !PyTuple_Check( chunk ) || !(PyTuple_Size( chunk ) == 2) ) {
        PyErr_SetString( PyExc_TypeError, "vector_read() expects list of tuples"
                                          "of length 2" );
        return NULL;
      }

      // Compute chunk size and make chunk
      uint64_t offset = PyInt_AsLong( PyTuple_GetItem( chunk, 0 ) );
      uint32_t length = PyInt_AsLong( PyTuple_GetItem( chunk, 1 ) );
      char    *buffer = new char[length];
      chunks.push_back( XrdCl::ChunkInfo( offset, length, buffer ) );
    }

    if ( callback && callback != Py_None ) {
      XrdCl::ResponseHandler *handler
        = GetHandler<XrdCl::VectorReadInfo>( callback );
      if ( !handler ) return NULL;
      async( status = self->file->VectorRead( chunks, 0, handler, timeout ) );
      return Py_BuildValue( "O", ConvertType<XrdCl::XRootDStatus>( &status ) );
    }

    else {
      XrdCl::VectorReadInfo *info = 0;
      status = self->file->VectorRead( chunks, 0, info, timeout );
      pyresponse = ConvertType<XrdCl::VectorReadInfo>( info );
      return Py_BuildValue( "OO", ConvertType<XrdCl::XRootDStatus>( &status ),
                                  pyresponse );
    }
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

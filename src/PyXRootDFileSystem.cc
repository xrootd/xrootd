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

#include "XrdCl/XrdClFileSystem.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Stat a path.
  //!
  //! This function can be synchronous or asynchronous, depending if a callback
  //! argument is given. The callback can be any Python callable.
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Stat( Client *self, PyObject *args )
  {
    const char *path;
    PyObject   *callback = NULL;
    PyObject   *responseBind = NULL;
    XrdCl::XRootDStatus status;
    XrdCl::FileSystem   filesystem( *self->url->url );

    //--------------------------------------------------------------------------
    // Parse the stat path and optional callback argument
    //--------------------------------------------------------------------------
    if ( !PyArg_ParseTuple( args, "s|O", &path, &callback ) )
      return NULL;

    //--------------------------------------------------------------------------
    // Asynchronous mode
    //--------------------------------------------------------------------------
    if ( callback ) {
      if (!IsCallable(callback)) return NULL;

      XrdCl::ResponseHandler *handler =
          new AsyncResponseHandler<XrdCl::StatInfo>( &StatInfoType, callback );

      //------------------------------------------------------------------------
      // Spin the async request (while releasing the GIL) and return None.
      //------------------------------------------------------------------------
      Py_BEGIN_ALLOW_THREADS
      status = filesystem.Stat( path, handler, 5 );
      Py_END_ALLOW_THREADS
    }

    //--------------------------------------------------------------------------
    // Synchronous mode
    //--------------------------------------------------------------------------
    else {
      XrdCl::StatInfo *response = 0;
      status = filesystem.Stat( path, response, 5 );

      //------------------------------------------------------------------------
      // Convert the response object, if any
      //------------------------------------------------------------------------
      if ( response ) {
        responseBind = ConvertType<XrdCl::StatInfo>( response, &StatInfoType );
      } else {
        responseBind = Py_None;
      }
    }

    //--------------------------------------------------------------------------
    // Convert the XRootDStatus object
    //--------------------------------------------------------------------------
    PyObject *statusDict = XRootDStatusDict(&status);
    if (!statusDict) return NULL;

    return (callback) ?
            Py_BuildValue( "O", statusDict ) :
            Py_BuildValue( "OO", statusDict, responseBind );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive
  //----------------------------------------------------------------------------
  PyObject* FileSystem::Ping( Client *self, PyObject *args, PyObject *kwds )
  {
    uint16_t timeout = 5;
    PyObject *callback = NULL;
    XrdCl::XRootDStatus status;
    XrdCl::FileSystem   filesystem( *self->url->url );
    static char *kwlist[] = { "timeout", "callback", NULL };

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "|IO", kwlist, &timeout, &callback ) )
      return NULL;

    if ( callback ) {
      if ( !IsCallable( callback ) )
        return NULL;

      XrdCl::ResponseHandler *handler =
          new AsyncResponseHandler<>( (PyTypeObject*) Py_None, callback );

      Py_BEGIN_ALLOW_THREADS
      status = filesystem.Ping( handler, timeout );
      Py_END_ALLOW_THREADS

      PyObject *statusDict = XRootDStatusDict( &status );
      if ( !statusDict )
        return NULL;
      return Py_BuildValue( "O", statusDict );
    }

    status = filesystem.Ping( timeout );

    PyObject *statusDict = XRootDStatusDict( &status );
    if ( !statusDict )
      return NULL;

    return Py_BuildValue( "O", statusDict );
  }
}

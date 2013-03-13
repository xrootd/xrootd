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
    static char *kwlist[]   = { "url", "flags", "mode", "timeout", "callback", NULL };
    const  char *url;
    uint16_t     flags = 0, mode = 0, timeout = 5;
    PyObject    *callback   = NULL;
    XrdCl::XRootDStatus status;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "s|HHHO", kwlist,
        &url, &flags, &mode, &timeout, &callback ) ) return NULL;

    // Asynchronous mode
    if ( callback ) {
      XrdCl::ResponseHandler *handler = GetHandler<XrdCl::AnyObject>( callback );
      if ( !handler ) return NULL;
      status = self->file->Open( url, flags, mode, handler, timeout );
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
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Obtain status information for this file
  //----------------------------------------------------------------------------
  PyObject* File::Stat( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Read a data chunk at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Read( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Write a data chank at a given offset
  //----------------------------------------------------------------------------
  PyObject* File::Write( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Commit all pending disk writes
  //----------------------------------------------------------------------------
  PyObject* File::Sync( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Truncate the file to a particular size
  //----------------------------------------------------------------------------
  PyObject* File::Truncate( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Read scattered data chunks in one operation
  //----------------------------------------------------------------------------
  PyObject* File::VectorRead( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Check if the file is open
  //----------------------------------------------------------------------------
  PyObject* File::IsOpen( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Enable/disable state recovery procedures while the file is open for
  //! reading
  //----------------------------------------------------------------------------
  PyObject* File::EnableReadRecovery( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Enable/disable state recovery procedures while the file is open for
  //! writing or read/write
  //----------------------------------------------------------------------------
  PyObject* File::EnableWriteRecovery( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }

  //----------------------------------------------------------------------------
  //! Get the data server the file is accessed at
  //----------------------------------------------------------------------------
  PyObject* File::GetDataServer( File *self, PyObject *args, PyObject *kwds )
  {
    PyErr_SetString(PyExc_NotImplementedError, "Method not implemented");
    return NULL;
  }
}

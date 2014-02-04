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

#include "PyXRootDCopyProgressHandler.hh"
#include "Conversions.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Notify when a new job is about to start
  //----------------------------------------------------------------------------
  void CopyProgressHandler::BeginJob( uint16_t          jobNum,
                                      uint16_t          jobTotal,
                                      const XrdCl::URL *source,
                                      const XrdCl::URL *target )
  {
    //--------------------------------------------------------------------------
    //! Acquire the GIL in case we are called from a non-Python thread
    //--------------------------------------------------------------------------
    PyGILState_STATE state = PyGILState_Ensure();

    URLType.tp_new = PyType_GenericNew;
    if ( PyType_Ready( &URLType ) < 0 ) return;
    Py_INCREF( &URLType );

    //--------------------------------------------------------------------------
    //! Build the Python URLs
    //--------------------------------------------------------------------------
    PyObject *pysource = PyObject_CallObject( (PyObject*) &URLType,
              Py_BuildValue( "(s)", source->GetURL().c_str() ) );
    if( PyErr_Occurred() ) PyErr_Print();

    PyObject *pytarget = PyObject_CallObject( (PyObject*) &URLType,
              Py_BuildValue( "(s)", target->GetURL().c_str() ) );
    if( PyErr_Occurred() ) PyErr_Print();

    //--------------------------------------------------------------------------
    //! Invoke the method
    //--------------------------------------------------------------------------
    if (handler != NULL)
    {
      PyObject_CallMethod( handler, const_cast<char*>( "begin" ),
                           (char *) "(HHOO)", jobNum, jobTotal,
                           pysource, pytarget );
    }

    //--------------------------------------------------------------------------
    //! Release the GIL
    //--------------------------------------------------------------------------
    PyGILState_Release(state);
  }

  //----------------------------------------------------------------------------
  //! Notify when the previous job has finished
  //----------------------------------------------------------------------------
  void CopyProgressHandler::EndJob( const XrdCl::XRootDStatus &status )
  {
    PyGILState_STATE state = PyGILState_Ensure();

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>
                         ( (XrdCl::XRootDStatus*) &status );
    //--------------------------------------------------------------------------
    //! Invoke the method
    //--------------------------------------------------------------------------
    if (handler != NULL)
    {
      PyObject_CallMethod( handler, const_cast<char*>( "end" ),
                           (char *) "O", pystatus );
    }

    PyGILState_Release(state);
  }

  //----------------------------------------------------------------------------
  //! Notify about the progress of the current job
  //----------------------------------------------------------------------------
  void CopyProgressHandler::JobProgress( uint64_t bytesProcessed,
                                         uint64_t bytesTotal )
  {
    PyGILState_STATE state = PyGILState_Ensure();

    if (handler != NULL)
    {
      PyObject_CallMethod( handler, const_cast<char*>( "update" ),
                           (char *) "kk", bytesProcessed, bytesTotal );
    }

    PyGILState_Release(state);
  }
}

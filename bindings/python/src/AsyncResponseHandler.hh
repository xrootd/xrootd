//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
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

#ifndef ASYNCRESPONSEHANDLER_HH_
#define ASYNCRESPONSEHANDLER_HH_

#include "PyXRootD.hh"
#include "Conversions.hh"
#include "Utils.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Generic asynchronous response handler
  //----------------------------------------------------------------------------
  template<typename Type>
  class AsyncResponseHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AsyncResponseHandler( PyObject *callback ) :
          callback( callback ), state( PyGILState_UNLOCKED ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~AsyncResponseHandler() {};

      //------------------------------------------------------------------------
      //! Handle the asynchronous response call
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject *response,
                                    XrdCl::HostList *hostList )
      {
        // If we get called while the program's exit handlers are being called,
        // then calls to PyGILState_Ensure() deadlock.  Py_IsInitialized() is
        // not thread-safe but we appear to be lacking in alternates.
        if (!Py_IsInitialized()) {return;}

        //----------------------------------------------------------------------
        // Ensure we hold the Global Interpreter Lock
        //----------------------------------------------------------------------
        state = PyGILState_Ensure();
        if ( InitTypes() != 0) {
          delete status;
          delete response;
          delete hostList;
          return Exit();
        }

        //----------------------------------------------------------------------
        // Convert the XRootDStatus object
        //----------------------------------------------------------------------
        PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( status );
        if ( !pystatus || PyErr_Occurred() ) {
          delete status;
          delete response;
          delete hostList;
          return Exit();
        }

        //----------------------------------------------------------------------
        // Convert the response object, if any
        //----------------------------------------------------------------------
        PyObject *pyresponse = NULL;
        if ( response != NULL) {
          pyresponse = ParseResponse( response );
          if ( pyresponse == NULL || PyErr_Occurred() ) {
            Py_XDECREF( pystatus );
            delete status;
            delete response;
            delete hostList;
            return Exit();
          }
        }

        //----------------------------------------------------------------------
        // Convert the host list
        //----------------------------------------------------------------------
        PyObject *pyhostlist = PyList_New( 0 );
        if ( hostList != NULL ) {
          pyhostlist = ConvertType<XrdCl::HostList>( hostList );
          if ( pyhostlist == NULL || PyErr_Occurred() ) {
            Py_XDECREF( pystatus );
            Py_XDECREF( pyresponse );
            delete status;
            delete response;
            delete hostList;
            return Exit();
          }
        }

        //----------------------------------------------------------------------
        // Build the callback arguments
        //----------------------------------------------------------------------
        if (pyresponse == NULL) pyresponse = Py_BuildValue( "" );
        PyObject *args = Py_BuildValue( "(OOO)", pystatus, pyresponse, pyhostlist );
        if ( !args || PyErr_Occurred() ) {
          Py_XDECREF( pystatus );
          Py_XDECREF( pyresponse );
          Py_XDECREF( pyhostlist );
          delete status;
          delete response;
          delete hostList;
          return Exit();
        }

        //----------------------------------------------------------------------
        //  Check if it is a final response or if it is just a chunk
        //----------------------------------------------------------------------
        bool finalrsp = !( status->IsOK() && status->code == XrdCl::suContinue );

        //----------------------------------------------------------------------
        // Invoke the Python callback
        //----------------------------------------------------------------------
        PyObject *callbackResult = PyObject_CallObject( this->callback, args );
        Py_DECREF( args );
        if ( !callbackResult || PyErr_Occurred() ) {
          Py_XDECREF( pystatus );
          Py_XDECREF( pyresponse );
          Py_XDECREF( pyhostlist );
          delete status;
          delete response;
          delete hostList;
          return Exit();
        }

        //----------------------------------------------------------------------
        // Clean up
        //----------------------------------------------------------------------
        Py_XDECREF( pystatus );
        Py_XDECREF( pyresponse );
        Py_XDECREF( pyhostlist );
        Py_XDECREF( callbackResult );
        if( finalrsp )
        {
          Py_XDECREF( this->callback );
        }

        PyGILState_Release( state );

        delete status;
        delete response;
        delete hostList;

        if( finalrsp )
          // Commit suicide...
          delete this;
      }

      //------------------------------------------------------------------------
      //! Handle the asynchronous response call
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
                           XrdCl::AnyObject *response )
      {
        // If we get called while the program's exit handlers are being called,
        // then calls to PyGILState_Ensure() deadlock.  Py_IsInitialized() is
        // not thread-safe but we appear to be lacking in alternates.
        if (!Py_IsInitialized()) {return;}

        //----------------------------------------------------------------------
        // Ensure we hold the Global Interpreter Lock
        //----------------------------------------------------------------------
        state = PyGILState_Ensure();
        if ( InitTypes() != 0) {
          return Exit();
        }

        //----------------------------------------------------------------------
        // Convert the XRootDStatus object
        //----------------------------------------------------------------------
        PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>( status );
        if ( !pystatus || PyErr_Occurred() ) {
          return Exit();
        }

        //----------------------------------------------------------------------
        // Convert the response object, if any
        //----------------------------------------------------------------------
        PyObject *pyresponse = NULL;
        if ( response != NULL) {
          pyresponse = ParseResponse( response );
          if ( pyresponse == NULL || PyErr_Occurred() ) {
            Py_XDECREF( pystatus );
            delete response;
            return Exit();
          }
        }

        //----------------------------------------------------------------------
        // Build the callback arguments
        //----------------------------------------------------------------------
        if (pyresponse == NULL) pyresponse = Py_BuildValue( "" );
        PyObject *args = Py_BuildValue( "(OO)", pystatus, pyresponse );
        if ( !args || PyErr_Occurred() ) {
          Py_XDECREF( pystatus );
          Py_XDECREF( pyresponse );
          delete response;
          return Exit();
        }

        //----------------------------------------------------------------------
        //  Check if it is a final response or if it is just a chunk
        //----------------------------------------------------------------------
        bool finalrsp = !( status->IsOK() && status->code == XrdCl::suContinue );

        //----------------------------------------------------------------------
        // Invoke the Python callback
        //----------------------------------------------------------------------
        PyObject *callbackResult = PyObject_CallObject( this->callback, args );
        Py_DECREF( args );
        if ( !callbackResult || PyErr_Occurred() ) {
          Py_XDECREF( pystatus );
          Py_XDECREF( pyresponse );
          delete response;
          return Exit();
        }

        //----------------------------------------------------------------------
        // Clean up
        //----------------------------------------------------------------------
        Py_XDECREF( pystatus );
        Py_XDECREF( pyresponse );
        Py_XDECREF( callbackResult );
        if( finalrsp )
        {
          Py_XDECREF( this->callback );
        }

        PyGILState_Release( state );

        delete status;
        delete response;

        if( finalrsp )
          // Commit suicide...
          delete this;
      }

      //------------------------------------------------------------------------
      //! Parse out and convert the AnyObject response to a mapping type
      //------------------------------------------------------------------------
      PyObject* ParseResponse( XrdCl::AnyObject *response )
      {
        PyObject *pyresponse = 0;
        Type *type;
        response->Get( type );
        pyresponse = ConvertType<Type>( type );
        return ( !pyresponse || PyErr_Occurred() ) ? NULL : pyresponse;
      }

      //------------------------------------------------------------------------
      //! Something went wrong, print error and release the GIL before returning
      //------------------------------------------------------------------------
      void Exit()
      {
        PyErr_Print();
        PyGILState_Release( state );
        delete this;
      }

    private:

      PyObject *callback;
      PyGILState_STATE state;
  };

  //----------------------------------------------------------------------------
  //! Get an async response handler of the correct type
  //----------------------------------------------------------------------------
  template<typename T>
  XrdCl::ResponseHandler* GetHandler( PyObject *callback )
  {
    if (!IsCallable(callback)) {
      return NULL;
    }

    return new AsyncResponseHandler<T>( callback );
  }
}

#endif /* ASYNCRESPONSEHANDLER_HH_ */

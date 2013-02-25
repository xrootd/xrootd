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

#ifndef ASYNCRESPONSEHANDLER_HH_
#define ASYNCRESPONSEHANDLER_HH_

#include <Python.h>

#include "XrdCl/XrdClXRootDResponses.hh"

#include "HostInfoType.hh"
#include "StatInfoType.hh"

namespace XrdClBind
{
  //----------------------------------------------------------------------------
  //! Generic asynchronous response handler
  //----------------------------------------------------------------------------
  template<class Type>
  class AsyncResponseHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AsyncResponseHandler(PyTypeObject *bindType, PyObject *callback):
        bindType(bindType),
        callback(callback) {}

      //------------------------------------------------------------------------
      //! Handle the asynchronous response call
      //------------------------------------------------------------------------
      void HandleResponseWithHosts(XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response, XrdCl::HostList *hostList)
      {
        //----------------------------------------------------------------------
        // Ensure we hold the Global Interpreter Lock
        //----------------------------------------------------------------------
        PyGILState_STATE state = PyGILState_Ensure();

        //----------------------------------------------------------------------
        // Convert the XRootDStatus object
        //----------------------------------------------------------------------
        PyObject *statusArgs = Py_BuildValue("(HHIs)", status->status,
                status->code, status->errNo, status->GetErrorMessage().c_str());
        if (!statusArgs || PyErr_Occurred()) {
          PyErr_Print(); PyGILState_Release(state);
          return;
        }

        PyObject *statusBind = PyObject_CallObject(
                                    (PyObject *) &XRootDStatusType, statusArgs);
        Py_DECREF(statusArgs);
        if (!statusBind || PyErr_Occurred()) {
          PyErr_Print(); PyGILState_Release(state);
          return;
        }

        //----------------------------------------------------------------------
        // Convert the response object, if any
        //----------------------------------------------------------------------
        PyObject *responseBind;
        if (response != NULL) {
          responseBind = ParseResponse(response);
          if (!responseBind || PyErr_Occurred()) {
            PyErr_Print(); PyGILState_Release(state);
            return;
          }
        } else {
          responseBind = Py_None;
        }

        //----------------------------------------------------------------------
        // Convert the host list
        //----------------------------------------------------------------------
        PyObject *hostListBind = PyList_New(0);

        for (unsigned int i = 0; i < hostList->size(); ++i) {
            PyObject *hostInfoArgs = Py_BuildValue("(O)",
                        PyCObject_FromVoidPtr((void *) &hostList->at(i), NULL));
            if (!hostInfoArgs || PyErr_Occurred()) {
              PyErr_Print(); PyGILState_Release(state);
              return;
            }

            PyObject *hostInfoBind = PyObject_CallObject(
                                     (PyObject *) &HostInfoType, hostInfoArgs);
            Py_DECREF(hostInfoArgs);
            if (!hostInfoBind || PyErr_Occurred()) {
              PyErr_Print(); PyGILState_Release(state);
              return;
            }

            Py_INCREF(hostInfoBind);
            if (PyList_Append(hostListBind, hostInfoBind) != 0) {
              PyErr_Print(); PyGILState_Release(state);
              return;
            }
        }

        //----------------------------------------------------------------------
        // Build the callback arguments
        //----------------------------------------------------------------------
        PyObject *args = Py_BuildValue("(OOO)", statusBind, responseBind,
            hostListBind);
        if (!args || PyErr_Occurred()) {
          PyErr_Print(); PyGILState_Release(state);
          return;
        }

        //----------------------------------------------------------------------
        // Invoke the Python callback
        //----------------------------------------------------------------------
        PyObject *callbackResult = PyObject_CallObject(this->callback, args);
        Py_DECREF(args);
        if (PyErr_Occurred()) {
          PyErr_Print(); PyGILState_Release(state);
          return;
        }

        //----------------------------------------------------------------------
        // Clean up
        //----------------------------------------------------------------------
        Py_XDECREF(statusBind);
        Py_XDECREF(responseBind);
        Py_XDECREF(hostListBind);
        Py_XDECREF(callbackResult);
        Py_DECREF(this->callback);

        delete status;
        delete response;
        delete hostList;
        // Commit suicide...
        delete this;
      }

      //------------------------------------------------------------------------
      //! Parse out and convert the AnyObject response to a mapping type
      //------------------------------------------------------------------------
      PyObject* ParseResponse(XrdCl::AnyObject *response)
      {
        PyObject *responseBind;
        Type     *type = 0;
        response->Get(type);

        //----------------------------------------------------------------------
        // Build the arguments for creating the response mapping type. We cast
        // the response object to a void * before packing it into a PyCObject.
        //
        // The CObject API is deprecated as of Python 2.7
        //----------------------------------------------------------------------
        PyObject *responseArgs = Py_BuildValue("(O)",
                                    PyCObject_FromVoidPtr((void *) type, NULL));
        if (!responseArgs) return NULL;

        //----------------------------------------------------------------------
        // Call the constructor of the bound type.
        //----------------------------------------------------------------------
        responseBind = PyObject_CallObject((PyObject *) this->bindType,
                                           responseArgs);

        return responseBind ? responseBind : NULL;
      }

    private:

      PyTypeObject *bindType;
      PyObject *callback;
  };
}

#endif /* ASYNCRESPONSEHANDLER_HH_ */

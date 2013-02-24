/*
 * AsyncResponseHandler.hh
 *
 *  Created on: Feb 24, 2013
 *      Author: root
 */

#ifndef ASYNCRESPONSEHANDLER_HH_
#define ASYNCRESPONSEHANDLER_HH_

#include <Python.h>

#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdClBind
{
    class AsyncResponseHandler : public XrdCl::ResponseHandler
    {
    public:
        AsyncResponseHandler(PyObject *callback) {
            this->callback = callback;
        }

        void HandleResponse(XrdCl::XRootDStatus* status,
                XrdCl::AnyObject* response) {
            std::cout << "HandleResponse() called, yay" << std::endl;

            PyObject *args = Py_BuildValue("(ss)", "foo", "bar");
            PyObject *result = PyObject_CallObject(this->callback, args);
            Py_DECREF(args);
        }

        PyObject *callback;
    };
}

#endif /* ASYNCRESPONSEHANDLER_HH_ */

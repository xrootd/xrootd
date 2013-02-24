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

#ifndef XRDCLBIND_HH_
#define XRDCLBIND_HH_

#include <Python.h>
#include <iostream>
#include "structmember.h"

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XRootDStatusType.hh"
#include "StatInfoType.hh"
#include "URLType.hh"

namespace XrdClBind
{
    //--------------------------------------------------------------------------
    //! Client binding type definition
    //--------------------------------------------------------------------------
    typedef struct {
        PyObject_HEAD
        /* Type-specific fields */
        URL *url;
    } Client;

    //--------------------------------------------------------------------------
    //! Deallocation function, called when object is deleted
    //--------------------------------------------------------------------------
    static void Client_dealloc(Client *self)
    {
        Py_XDECREF(self->url);
        self->ob_type->tp_free((PyObject*) self);
    }

    //--------------------------------------------------------------------------
    //! __init__() equivalent
    //--------------------------------------------------------------------------
    static int Client_init(Client *self, PyObject *args, PyObject *kwds)
    {
        const char *urlstr;
        static char *kwlist[] = {"url", NULL};

        if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &urlstr))
            return -1;

        PyObject *bind_args = Py_BuildValue("(s)", urlstr);
        if (!bind_args) {
            return NULL;
        }

        self->url = (URL*) PyObject_CallObject((PyObject*) &URLType, bind_args);
        Py_DECREF(bind_args);

        if (!self->url) {
            return NULL;
        }

        return 0;
    }

    //--------------------------------------------------------------------------
    //! Stat a path and return the XRootDStatus and StatInfo mapping types
    //--------------------------------------------------------------------------
    static PyObject* Stat(Client *self, PyObject *args)
    {
        const char *path;

        if (!PyArg_ParseTuple(args, "s", &path))
            return NULL;

        XrdCl::FileSystem fs(*self->url->url);
        XrdCl::XRootDStatus status;
        XrdCl::StatInfo *response = 0;

        status = fs.Stat(path, response, 5);

        // Build XRootDStatus mapping object
        PyObject *status_args = Py_BuildValue("(HHIs)", status.status,
                status.code, status.errNo, status.GetErrorMessage().c_str());
        if (!status_args) {
            return NULL;
        }

        PyObject *status_bind = PyObject_CallObject((PyObject *)
                &XRootDStatusType, status_args);
        if (!status_bind) {
            return NULL;
        }
        Py_DECREF(status_args);

        // Build StatInfo mapping object (if we got one)
        PyObject* response_bind;
        if (response) {

            // Ugly, StatInfo should have constructor for this
            std::ostringstream data;
            data << response->GetId() << " ";
            data << response->GetSize() << " ";
            data << response->GetFlags() << " ";
            data << response->GetModTime();

            PyObject* response_args = Py_BuildValue("(s)", data.str().c_str());
            if (!response_args) {
                return NULL;
            }

            response_bind = PyObject_CallObject((PyObject *) &StatInfoType,
                    response_args);
            if (!response_bind) {
                return NULL;
            }

            Py_DECREF(response_args);
        }

        return Py_BuildValue("OO", status_bind, response_bind);
    }

    //--------------------------------------------------------------------------
    //! Visible member definitions
    //--------------------------------------------------------------------------
    static PyMemberDef ClientMembers[] = {
        {"url", T_OBJECT_EX, offsetof(Client, url), 0,
         "Server URL"},
        {NULL}  /* Sentinel */
    };

    //--------------------------------------------------------------------------
    //! Visible method definitions
    //--------------------------------------------------------------------------
    static PyMethodDef ClientMethods[] = {
        {"stat", (PyCFunction) Stat, METH_VARARGS,
         "Stat a path"},
        {NULL}  /* Sentinel */
    };

    //--------------------------------------------------------------------------
    //! Client binding type object
    //--------------------------------------------------------------------------
    static PyTypeObject ClientType = {
        PyObject_HEAD_INIT(NULL)
        0,                                          /* ob_size */
        "client.Client",                            /* tp_name */
        sizeof(Client),                             /* tp_basicsize */
        0,                                          /* tp_itemsize */
        (destructor) Client_dealloc,                /* tp_dealloc */
        0,                                          /* tp_print */
        0,                                          /* tp_getattr */
        0,                                          /* tp_setattr */
        0,                                          /* tp_compare */
        0,                                          /* tp_repr */
        0,                                          /* tp_as_number */
        0,                                          /* tp_as_sequence */
        0,                                          /* tp_as_mapping */
        0,                                          /* tp_hash */
        0,                                          /* tp_call */
        0,                                          /* tp_str */
        0,                                          /* tp_getattro */
        0,                                          /* tp_setattro */
        0,                                          /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
        "Client object",                            /* tp_doc */
        0,                                          /* tp_traverse */
        0,                                          /* tp_clear */
        0,                                          /* tp_richcompare */
        0,                                          /* tp_weaklistoffset */
        0,                                          /* tp_iter */
        0,                                          /* tp_iternext */
        ClientMethods,                              /* tp_methods */
        ClientMembers,                              /* tp_members */
        0,                                          /* tp_getset */
        0,                                          /* tp_base */
        0,                                          /* tp_dict */
        0,                                          /* tp_descr_get */
        0,                                          /* tp_descr_set */
        0,                                          /* tp_dictoffset */
        (initproc) Client_init,                     /* tp_init */
    };
}

#endif /* XRDCLBIND_HH_ */

/*
 * XRootDStatusType.hh
 *
 *  Created on: Feb 20, 2013
 *      Author: root
 */

#ifndef XROOTDSTATUSTYPE_HH_
#define XROOTDSTATUSTYPE_HH_

#include <Python.h>
#include "structmember.h"

#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdClBind
{
    typedef struct {
        PyObject_HEAD
        /* Type-specific fields */
        uint16_t status;
        uint16_t code;
        uint32_t errNo;
    } XRootDStatus;

    static void
    XRootDStatus_dealloc(XRootDStatus* self)
    {
        self->ob_type->tp_free((PyObject*) self);
    }

    static int
    XRootDStatus_init(XRootDStatus *self, PyObject *args, PyObject *kwds) {
        std::cout << "XRootDStatus_init" << std::endl;
        static char *kwlist[] = {"status", "code", "errNo", NULL};

        if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii", kwlist,
                &self->status, &self->code, &self->errNo))
            return -1;

        return 0;
    }

    static PyObject*
    IsError(XRootDStatus* self)
    {
        PyObject *status = Py_BuildValue("i", self->status);
        if (status == NULL)
            return NULL;

        Py_DECREF(status);
        return status;
    }

    static PyMemberDef XRootDStatusMembers[] = {
        {"status", T_INT, offsetof(XRootDStatus, status), 0,
         "Status of the execution"},
        {"code", T_INT, offsetof(XRootDStatus, code), 0,
         "Error type, or additional hints on what to do"},
        {"errNo", T_INT, offsetof(XRootDStatus, errNo), 0,
         "Errno, if any"},
        {NULL}  /* Sentinel */
    };

    static PyMethodDef XRootDStatusMethods[] = {
        {"IsError", (PyCFunction) IsError, METH_NOARGS,
         "Return the error status"},
        {NULL}  /* Sentinel */
    };

    static PyTypeObject XRootDStatusType = {
        PyObject_HEAD_INIT(NULL)
        0,                                          /* ob_size */
        "client.XRootDStatus",                      /* tp_name */
        sizeof(XRootDStatus),                       /* tp_basicsize */
        0,                                          /* tp_itemsize */
        (destructor) XRootDStatus_dealloc,          /* tp_dealloc */
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
        "XRootDStatus object",                      /* tp_doc */
        0,                                          /* tp_traverse */
        0,                                          /* tp_clear */
        0,                                          /* tp_richcompare */
        0,                                          /* tp_weaklistoffset */
        0,                                          /* tp_iter */
        0,                                          /* tp_iternext */
        XRootDStatusMethods,                        /* tp_methods */
        XRootDStatusMembers,                        /* tp_members */
        0,                                          /* tp_getset */
        0,                                          /* tp_base */
        0,                                          /* tp_dict */
        0,                                          /* tp_descr_get */
        0,                                          /* tp_descr_set */
        0,                                          /* tp_dictoffset */
        (initproc) XRootDStatus_init,               /* tp_init */
    };
}

#endif /* XROOTDSTATUSTYPE_HH_ */

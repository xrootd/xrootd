#ifndef URLTYPE_HH_
#define URLTYPE_HH_

#include <Python.h>
#include "structmember.h"

namespace XrdClBind
{

    typedef struct {
        PyObject_HEAD
        /* Type-specific fields go here. */
        const char *url;
    } URL;

    static void
    URL_dealloc(URL* self)
    {
        //Py_XDECREF(self->url);
        self->ob_type->tp_free((PyObject*) self);
    }

    static int
    URL_init(URL *self, PyObject *args, PyObject *kwds) {
        std::cout << "URL_init" << std::endl;

        const char *url;
        static char *kwlist[] = {"url", NULL};

        if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &url))
            return -1;

        if (url) {
            self->url = url;
        }

        return 0;
    }

    static PyObject*
    IsValid(URL* self)
    {
        PyObject *valid = Py_BuildValue("s", "not implemented");
        if (valid == NULL)
            return NULL;

        return valid;
    }

    static PyMemberDef URLMembers[] = {
        {"url", T_STRING, offsetof(URL, url), 0,
         "The actual URL"},
        {NULL}  /* Sentinel */
    };

    static PyMethodDef URLMethods[] = {
        {"IsValid", (PyCFunction) IsValid, METH_NOARGS,
         "Return the validity of the URL"},
        {NULL}  /* Sentinel */
    };

    static PyTypeObject URLType = {
        PyObject_HEAD_INIT(NULL)
        0,                                          /* ob_size */
        "client.URL",                               /* tp_name */
        sizeof(URL),                                /* tp_basicsize */
        0,                                          /* tp_itemsize */
        (destructor) URL_dealloc,                   /* tp_dealloc */
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
        "URL object",                               /* tp_doc */
        0,                                          /* tp_traverse */
        0,                                          /* tp_clear */
        0,                                          /* tp_richcompare */
        0,                                          /* tp_weaklistoffset */
        0,                                          /* tp_iter */
        0,                                          /* tp_iternext */
        URLMethods,                                 /* tp_methods */
        URLMembers,                                 /* tp_members */
        0,                                          /* tp_getset */
        0,                                          /* tp_base */
        0,                                          /* tp_dict */
        0,                                          /* tp_descr_get */
        0,                                          /* tp_descr_set */
        0,                                          /* tp_dictoffset */
        (initproc) URL_init,                        /* tp_init */
    };
}

#endif /* URLTYPE_HH_ */

#ifndef PYXROOTDINFO_HH
#define PYXROOTDINFO_HH

#include "PyXRootD.hh"

namespace PyXRootD
{
class Info
{
public:
    static PyObject *showDate(PyObject *self);

public:
    PyObject_HEAD
};

//----------------------------------------------------------------------------
//! Visible method definitions
//----------------------------------------------------------------------------
static PyMethodDef InfoMethods[] =
    {
        {"showdate",
         (PyCFunction)PyXRootD::Info::showDate, METH_NOARGS, NULL},
        {NULL} /* Sentinel */
};

//----------------------------------------------------------------------------
//! Visible member definitions
//----------------------------------------------------------------------------
static PyMemberDef InfoMembers[] =
    {
        {NULL} /* Sentinel */
};

//----------------------------------------------------------------------------
//! File binding type object
//----------------------------------------------------------------------------
static PyTypeObject InfoType = {
    PyVarObject_HEAD_INIT(NULL, 0) "pyxrootd.Info",                  /* tp_name */
    sizeof(Info),                                                    /* tp_basicsize */
    0,                                                               /* tp_itemsize */
    0,                                                               /* tp_dealloc */
    0,                                                               /* tp_print */
    0,                                                               /* tp_getattr */
    0,                                                               /* tp_setattr */
    0,                                                               /* tp_compare */
    0,                                                               /* tp_repr */
    0,                                                               /* tp_as_number */
    0,                                                               /* tp_as_sequence */
    0,                                                               /* tp_as_mapping */
    0,                                                               /* tp_hash */
    0,                                                               /* tp_call */
    0,                                                               /* tp_str */
    0,                                                               /* tp_getattro */
    0,                                                               /* tp_setattro */
    0,                                                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_ITER, /* tp_flags */
    "CLASS INFO DOC",                                                /* tp_doc */
    0,                                                               /* tp_traverse */
    0,                                                               /* tp_clear */
    0,                                                               /* tp_richcompare */
    0,                                                               /* tp_weaklistoffset */
    0,                                                               /* tp_iter */
    0,                                                               /* tp_iternext */
    InfoMethods,                                                     /* tp_methods */
    InfoMembers,                                                     /* tp_members */
    0,                                                               /* tp_getset */
    0,                                                               /* tp_base */
    0,                                                               /* tp_dict */
    0,                                                               /* tp_descr_get */
    0,                                                               /* tp_descr_set */
    0,                                                               /* tp_dictoffset */
    0,                                                               /* tp_init */
};
} // namespace PyXRootD

#endif // PYXROOTDINFO_HH

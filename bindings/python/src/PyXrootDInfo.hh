#ifndef PYXROOTDINFO_HH
#define PYXROOTDINFO_HH

#include "PyXRootD.hh"

namespace PyXRootD
{
class Info
{
public:
    static PyObject *showDate(Info *self);
    static PyObject *showClass(Info *self);

public:
    PyObject_HEAD int number;
};

//THE NEW IMPLEMENTATION
static PyObject *Info_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Info *self;

    self = (Info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->number = 0;
    }
    return (PyObject *)self;
}

//THE INIT IMPLEMENATION
static int Info_init(Info *self, PyObject *args, PyObject *kwds)
{
    int initNumber;
    if (!PyArg_ParseTuple(args, "i", &initNumber))
    {
        return NULL;
    }
    self->number = initNumber;
    return 0;
}

//----------------------------------------------------------------------------
//! Visible method definitions
//----------------------------------------------------------------------------
static PyMethodDef InfoMethods[] =
    {
        {"showdate",
         (PyCFunction)PyXRootD::Info::showDate, METH_NOARGS, NULL},
        {"showclass",
         (PyCFunction)PyXRootD::Info::showClass, METH_NOARGS, NULL},
        {NULL} /* Sentinel */
};

// //class variables
// static PyMemberDef ArrayOps_members[] = {
//     {"number", T_INT, offsetof(ArrayOps, number), 0, "ArrayOps number variable type."},
//     {"size", T_INT, offsetof(ArrayOps, size), 0, "ArrayOps size of array variable type."},
//     {"array", T_OBJECT, offsetof(ArrayOps, arr), 0, "ArrayOps array variable type."},
//     {NULL} /* Sentinel */
// };

//----------------------------------------------------------------------------
//! Visible member definitions
//----------------------------------------------------------------------------
static PyMemberDef InfoMembers[] =
    {
        {"number", T_INT, offsetof(Info, number), 0, "Info number variable type."},
        {NULL} /* Sentinel */
};

//THE DEALLOC PROCEDURE
static void Info_dealloc(Info *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject InfoType = {
    PyVarObject_HEAD_INIT(NULL, 0) "pyxrootd.Info", /* tp_name */
    sizeof(Info),                                   /* tp_basicsize */
    0,                                              /* tp_itemsize */
    (destructor)Info_dealloc,                       /* tp_dealloc */
    0,                                              /* tp_print */
    0,                                              /* tp_getattr */
    0,                                              /* tp_setattr */
    0,                                              /* tp_reserved */
    0,                                              /* tp_repr */
    0,                                              /* tp_as_number */
    0,                                              /* tp_as_sequence */
    0,                                              /* tp_as_mapping */
    0,                                              /* tp_hash  */
    0,                                              /* tp_call */
    0,                                              /* tp_str */
    0,                                              /* tp_getattro */
    0,                                              /* tp_setattro */
    0,                                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,       /* tp_flags */
    "info xrootd objects",                          /* tp_doc */
    0,                                              /* tp_traverse */
    0,                                              /* tp_clear */
    0,                                              /* tp_richcompare */
    0,                                              /* tp_weaklistoffset */
    0,                                              /* tp_iter */
    0,                                              /* tp_iternext */
    InfoMethods,                                    /* tp_methods */
    InfoMembers,                                    /* tp_members */
    0,                                              /* tp_getset */
    0,                                              /* tp_base */
    0,                                              /* tp_dict */
    0,                                              /* tp_descr_get */
    0,                                              /* tp_descr_set */
    0,                                              /* tp_dictoffset */
    (initproc)Info_init,                            /* tp_init */
    0,                                              /* tp_alloc */
    Info_new,                                       /* tp_new */
};
/* 
static struct PyModuleDef Info_definition = {
    PyModuleDef_HEAD_INIT,
    "info",
    "info module for xrootd",
    -1,
    NULL,
};

PyMODINIT_FUNC PyInit_Info(void)
{
    Py_Initialize();
    PyObject *m = PyModule_Create(&Info_definition);

    if (PyType_Ready(&InfoType) < 0)
        return NULL;

    Py_INCREF(&InfoType);
    PyModule_AddObject(m, "Info", (PyObject *)&InfoType);

    return m;
} */

} // namespace PyXRootD

#endif // PYXROOTDINFO_HH

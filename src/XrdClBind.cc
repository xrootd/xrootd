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

#include <Python.h>

#include "ClientType.hh"
#include "XRootDStatusType.hh"

namespace XrdClBind
{
    //--------------------------------------------------------------------------
    //! Visible module method declarations
    //--------------------------------------------------------------------------
    static PyMethodDef module_methods[] = {
        {NULL}  /* Sentinel */
    };

    //--------------------------------------------------------------------------
    //! Module initialization function
    //--------------------------------------------------------------------------
    PyMODINIT_FUNC initclient(void)
    {
        PyObject* module;

        Py_Initialize();
        if (!PyEval_ThreadsInitialized()) {
            PyEval_InitThreads();
        }

        ClientType.tp_new = PyType_GenericNew;
        if (PyType_Ready(&ClientType) < 0)
            return;

        XRootDStatusType.tp_new = PyType_GenericNew;
        if (PyType_Ready(&XRootDStatusType) < 0)
            return;

        StatInfoType.tp_new = PyType_GenericNew;
        if (PyType_Ready(&StatInfoType) < 0)
            return;

        URLType.tp_new = PyType_GenericNew;
        if (PyType_Ready(&URLType) < 0)
            return;

        module = Py_InitModule3("client", module_methods,
                "Client extension module type.");

        Py_INCREF(&URLType);
        PyModule_AddObject(module, "URL", (PyObject *) &URLType);
        Py_INCREF(&StatInfoType);
        PyModule_AddObject(module, "StatInfo", (PyObject *) &StatInfoType);
        Py_INCREF(&XRootDStatusType);
        PyModule_AddObject(module, "XRootDStatus", (PyObject *) &XRootDStatusType);
        Py_INCREF(&ClientType);
        PyModule_AddObject(module, "Client", (PyObject *) &ClientType);
    }
}

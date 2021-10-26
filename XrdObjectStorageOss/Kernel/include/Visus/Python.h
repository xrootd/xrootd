#/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_PYTHON_H__
#define VISUS_PYTHON_H__


#pragma push_macro("slots")
#undef slots

#if defined(WIN32) && defined(_DEBUG) 
#undef _DEBUG
#include <Python.h>
#define _DEBUG 1
#else
#include <Python.h>
#endif

#pragma pop_macro("slots")

#include <string>
#include <sstream>
#include <vector>
#include <iostream>

#include <Visus/Kernel.h>
#include <Visus/StringUtils.h>
#include <Visus/Utils.h>
#include <Visus/Path.h>

////////////////////////////////////////////////////////////////////////////
inline PyThreadState*& EmbeddedPythonThreadState() {
  static PyThreadState* ret = nullptr;
  return ret;
}

////////////////////////////////////////////////////////////////////////////
inline void EmbeddedPythonInit()
{
  using namespace Visus;
  static String program_name = Path(Utils::getCurrentApplicationFile()).toString();

  PrintInfo("Embedding python...");

#if PY_VERSION_HEX >= 0x03050000
  static const wchar_t* wprogram_name=Py_DecodeLocale((char*)program_name.c_str(), NULL);
#else
  static const wchar_t* wprogram_name=_Py_char2wchar((char*)program_name.c_str(), NULL);
#endif

  Py_SetProgramName((wchar_t*)wprogram_name);
  Py_InitializeEx(0);

  static wchar_t* wargv[]={(wchar_t*)wprogram_name};
  PySys_SetArgv(1, wargv);

  //Initialize and acquire the global interpreter lock
  PyEval_InitThreads();

  //sometimes PyEval_InitThreads in apache/mod_visus don't get the GIL (python fatal error: PyEval_SaveThread: NULL tstate)
  if (auto old_state = PyThreadState_Swap(nullptr))
  {
    PyThreadState_Swap(old_state);
    EmbeddedPythonThreadState() = PyEval_SaveThread(); //this release the GIL
  }

  //evaluate some python commands
  {
    auto acquire_gil = PyGILState_Ensure();
    std::ostringstream out;
    out << "import os, sys\n";
    out << "sys.path.append(os.path.realpath('" + Path(program_name).getParent().toString() + "/../.." + "'))\n";
    PyRun_SimpleString(out.str().c_str());

    PyObject* py_main = PyImport_AddModule("__main__"); // Borrowed reference.
    PyObject* py_dict = PyModule_GetDict(py_main); //Borrowed reference.

    PyObject* obj = PyRun_String("'{}.{}'.format(sys.version_info.major, sys.version_info.minor)", Py_eval_input, py_dict, NULL); // Owned reference
    VisusReleaseAssert(obj != nullptr);

    PyObject* temp_bytes = PyUnicode_AsEncodedString(obj, "UTF-8", "strict"); // Owned reference
    VisusReleaseAssert(temp_bytes != NULL);
    String s = PyBytes_AS_STRING(temp_bytes); 

    //this can happen if you mix python 2.7 with 3.x (as it can happen with apache mod_visus with wsgi enabled)
    if (s != concatenate(cstring(PY_MAJOR_VERSION), ".", cstring(PY_MINOR_VERSION)))
      ThrowException("internal error, python returned a wrong version",s);

    Py_DECREF(obj);
    Py_DECREF(temp_bytes);

    PyRun_SimpleString("import sys; print(sys.executable,sys.argv)");

    PyGILState_Release(acquire_gil);
  }
}

////////////////////////////////////////////////////////////////////////////
inline void EmbeddedPythonShutdown()
{
  if (auto& thread_state = EmbeddedPythonThreadState())
  {
    PyEval_RestoreThread(thread_state);
    Py_Finalize();
    thread_state = nullptr;
  }
}


#endif //VISUS_PYTHON_H__


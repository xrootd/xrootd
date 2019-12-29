#include "PyXrootDInfo.hh"

#include <iostream> //for verbose testing

#include <chrono>
#include <string>
#include <ctime>

namespace PyXRootD
{
PyObject *Info::showDate(PyObject *self)
{
    const char text[] = {"Test"};
    PyObject *results = Py_BuildValue("s", text);
    return results;
}
} // namespace PyXrootDInfo
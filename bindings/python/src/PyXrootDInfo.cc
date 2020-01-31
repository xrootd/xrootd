#include "PyXrootDInfo.hh"

#include <iostream> //for verbose testing

#include <chrono>
#include <string>
#include <ctime>

namespace PyXRootD
{
PyObject *Info::showDate(Info *self)
{
    std::string text = "name";
    PyObject *results = Py_BuildValue("si", text.c_str(),self->number);
    return results;
}
PyObject *Info::showClass(Info *self)
{
    std::string text = "the class";
    PyObject *result = Py_BuildValue("s", text.c_str());
    return result;
}
} // namespace PyXRootD
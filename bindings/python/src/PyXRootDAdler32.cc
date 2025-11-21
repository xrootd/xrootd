//------------------------------------------------------------------------------
// Copyright (c) 2025 - Binding for setXAttrAdler32
//------------------------------------------------------------------------------
// This file provides a Python wrapper with simplified API for logic equivalent
// to fSetXattrAdler32(const char* path, int fd, const char* attr, char* value)
// present in Xrdadler32.cc.
//
// Simplified API: setXAttrAdler32(path, checksum)
//   - Takes only path and checksum string
//   - Opens file internally with O_RDONLY
//   - Uses fixed attribute names per original code
//------------------------------------------------------------------------------
#include "PyXRootDAdler32.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <time.h>
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
  #include <sys/xattr.h>
#endif

#include "XrdCks/XrdCksXAttr.hh"
#include "XrdOuc/XrdOucXAttr.hh"

namespace PyXRootD {

// Python exposed function: setXAttrAdler32(path:str, checksum:str)
// Returns None on success, raises OSError/ValueError on error.
extern "C" PyObject* setXAttrAdler32_cpp(PyObject* /*self*/, PyObject* args)
{
  const char* path = nullptr;
  const char* value = nullptr;

  if (!PyArg_ParseTuple(args, "ss", &path, &value)) {
    return nullptr; // Type error automatically set by PyArg_ParseTuple
  }

  // Basic validation matching original function expectations.
  if (!value || std::strlen(value) != 8) {
    PyErr_SetString(PyExc_ValueError, "Checksum value must be exactly 8 hex characters");
    return nullptr;
  }

  // Open file read-only (matching xrdadler32 usage pattern)
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    return nullptr;
  }

  // Stat the file via fd (original code uses fstat on fd only).
  struct stat st;
  if (fstat(fd, &st) != 0) {
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    return nullptr;
  }

  // Set up attribute object and populate checksum metadata.
  XrdOucXAttr<XrdCksXAttr> xCS;
  if (!xCS.Attr.Cks.Set("adler32") || !xCS.Attr.Cks.Set(value, 8)) {
    close(fd);
    PyErr_SetString(PyExc_RuntimeError, "Failed to set checksum name or value (invalid hex?)");
    return nullptr;
  }

  xCS.Attr.Cks.fmTime = static_cast<long long>(st.st_mtime);
  xCS.Attr.Cks.csTime = static_cast<int>(time(nullptr) - st.st_mtime);

  // Write the structured attribute using XrdSysFAttr mechanism via fd.
  int rc = xCS.Set("", fd);
  if (rc != 0) {
    int saved_errno = (rc < 0) ? -rc : errno;
    close(fd);
    // xCS.Set returns -errno on failure
    if (rc < 0) {
      errno = saved_errno;
      PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    } else {
      PyErr_SetString(PyExc_RuntimeError, "Unknown error writing checksum attribute");
    }
    return nullptr;
  }

  // Remove legacy attribute if present (original code did unconditional remove).
  // Use fixed attribute name per original code
  const char* legacy_attr = "user.checksum.adler32";
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
  (void)fremovexattr(fd, legacy_attr); // best-effort, ignore errors
#elif defined(__solaris__)
  int attrfd = openat(fd, legacy_attr, O_XATTR|O_RDONLY);
  if (attrfd >= 0) { unlinkat(attrfd, legacy_attr, 0); close(attrfd); }
#endif

  close(fd);
  Py_RETURN_NONE;
}

} // namespace PyXRootD

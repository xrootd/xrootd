/******************************************************************************/
/*                                                                            */
/*                    P y X R o o t D T a p e R e s t . c c                   */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#include "PyXRootDTapeRest.hh"
#include "Conversions.hh"
#include "XrdCl/XrdClTapeRest.hh"

#include <vector>

namespace
{
XrdCl::TapeRestOptions OptionsFromArgs(int timeout, const char *cert,
                                       const char *key, unsigned int verbosity)
{
  XrdCl::TapeRestOptions options;
  options.timeout = timeout;
  options.cert = cert ? cert : "";
  options.key = key ? key : "";
  options.verbosity = verbosity;
  return options;
}

PyObject *EndpointToDict(const XrdCl::TapeRestEndpoint &endpoint)
{
  return Py_BuildValue("{ssssss}",
    "uri", endpoint.uri.c_str(),
    "version", endpoint.version.c_str(),
    "sitename", endpoint.sitename.c_str());
}

PyObject *ArchiveInfoToDict(const XrdCl::TapeRestArchiveInfo &info)
{
  return Py_BuildValue("{ssssssss}",
    "url", info.url.c_str(),
    "path", info.path.c_str(),
    "locality", XrdCl::TapeRestClient::LocalityToString(info.locality).c_str(),
    "error", info.error.c_str());
}

bool SequenceToUrls(PyObject *sequence, std::vector<std::string> &urls)
{
  PyObject *fast = PySequence_Fast(sequence, "urls must be a sequence");
  if(!fast) return false;

  const Py_ssize_t size = PySequence_Fast_GET_SIZE(fast);
  urls.reserve(static_cast<std::size_t>(size));
  for(Py_ssize_t i = 0; i < size; ++i)
  {
    PyObject *item = PySequence_Fast_GET_ITEM(fast, i);
    const char *url = PyUnicode_AsUTF8(item);
    if(!url)
    {
      Py_DECREF(fast);
      return false;
    }
    urls.emplace_back(url);
  }

  Py_DECREF(fast);
  return true;
}

}

namespace PyXRootD
{
  PyObject* TapeRestDiscover_cpp( PyObject *self, PyObject *args )
  {
    char *url = nullptr;
    int timeout = -1;
    char *cert = const_cast<char *>("");
    char *key = const_cast<char *>("");
    unsigned int verbosity = 0;

    if(!PyArg_ParseTuple(args, "s|issI", &url, &timeout, &cert, &key,
                         &verbosity))
    {
      return nullptr;
    }

    XrdCl::TapeRestClient client(
      OptionsFromArgs(timeout, cert, key, verbosity));
    XrdCl::TapeRestEndpoint endpoint;
    XrdCl::XRootDStatus status = client.Discover(url, endpoint);

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>(&status);
    PyObject *pyendpoint = status.IsOK() ? EndpointToDict(endpoint)
                                         : Py_BuildValue("");
    return Py_BuildValue("NN", pystatus, pyendpoint);
  }

  PyObject* TapeRestArchiveInfo_cpp( PyObject *self, PyObject *args )
  {
    PyObject *pyurls = nullptr;
    int timeout = -1;
    char *cert = const_cast<char *>("");
    char *key = const_cast<char *>("");
    unsigned int verbosity = 0;

    if(!PyArg_ParseTuple(args, "O|issI", &pyurls, &timeout, &cert, &key,
                         &verbosity))
    {
      return nullptr;
    }

    std::vector<std::string> urls;
    if(!SequenceToUrls(pyurls, urls))
    {
      return nullptr;
    }

    XrdCl::TapeRestClient client(
      OptionsFromArgs(timeout, cert, key, verbosity));
    std::vector<XrdCl::TapeRestArchiveInfo> results;
    XrdCl::XRootDStatus status = client.ArchiveInfo(urls, results);

    PyObject *pyresults = PyList_New(results.size());
    for(std::size_t i = 0; i < results.size(); ++i)
    {
      PyList_SET_ITEM(pyresults, i, ArchiveInfoToDict(results[i]));
    }

    PyObject *pystatus = ConvertType<XrdCl::XRootDStatus>(&status);
    return Py_BuildValue("NN", pystatus, pyresults);
  }
}

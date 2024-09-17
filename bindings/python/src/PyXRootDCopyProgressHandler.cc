//------------------------------------------------------------------------------
// Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "PyXRootDCopyProgressHandler.hh"
#include "Conversions.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  // Notify when a new job is about to start
  //----------------------------------------------------------------------------
  void CopyProgressHandler::BeginJob( uint32_t          jobNum,
                                      uint32_t          jobTotal,
                                      const XrdCl::URL *source,
                                      const XrdCl::URL *target )
  {
    PyGILState_STATE state = PyGILState_Ensure();

    PyObject *ret = 0;
    if (handler != NULL)
    {
      ret = PyObject_CallMethod( handler, (char*)"begin", (char*)"(HHss)",
                                 jobNum, jobTotal, source->GetURL().c_str(),
                                 target->GetURL().c_str() );
      Py_XDECREF(ret);
    }

    PyGILState_Release(state);
  }

  //----------------------------------------------------------------------------
  // Notify when the previous job has finished
  //----------------------------------------------------------------------------
  void CopyProgressHandler::EndJob( uint32_t                   jobNum,
                                    const XrdCl::PropertyList *result )
  {
    PyGILState_STATE  state    = PyGILState_Ensure();
    PyObject         *pyresult = ConvertType(result);
    PyObject         *ret      = 0;

    if (handler)
    {
      ret = PyObject_CallMethod( handler, (char*)"end", (char*)"HO",
                                 jobNum, pyresult );
      Py_XDECREF(ret);
    }
    Py_XDECREF(pyresult);
    PyGILState_Release(state);
  }

  //----------------------------------------------------------------------------
  // Notify about the progress of the current job
  //----------------------------------------------------------------------------
  void CopyProgressHandler::JobProgress( uint32_t jobNum,
                                         uint64_t bytesProcessed,
                                         uint64_t bytesTotal )
  {
    PyGILState_STATE  state = PyGILState_Ensure();
    PyObject         *ret   = 0;
    if(handler)
    {
      ret = PyObject_CallMethod( handler, (char*)"update", (char*)"HKK",
                                 jobNum, bytesProcessed, bytesTotal );
      Py_XDECREF(ret);
    }

    PyGILState_Release(state);
  }

  //----------------------------------------------------------------------------
  // Check if the job should be canceled
  //----------------------------------------------------------------------------
  bool CopyProgressHandler::ShouldCancel( uint32_t jobNum )
  {
    PyGILState_STATE state = PyGILState_Ensure();
    bool             ret   = false;
    if(handler)
    {
      PyObject *val = PyObject_CallMethod( handler, (char*)"should_cancel",
                                           (char*)"H", jobNum );
      if(val)
      {
        if(PyBool_Check(val))
          ret = (val == Py_True);
        Py_DECREF(val);
      }
    }
    PyGILState_Release(state);
    return ret;
  }
}

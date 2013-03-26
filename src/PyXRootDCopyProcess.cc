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

#include "PyXRootDCopyProcess.hh"
#include "PyXRootDCopyJob.hh"

namespace PyXRootD
{
  PyObject* CopyProcess::AddJob( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    CopyJob *job;

    if ( !PyArg_ParseTuple( args, "O", &job ) ) return NULL;

    self->process->AddJob( job->job );
  }

  PyObject* CopyProcess::Prepare( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    XrdCl::XRootDStatus status = self->process->Prepare();
  }

  PyObject* CopyProcess::Run( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    XrdCl::CopyProgressHandler handler = new XrdCl::CopyProgressHandler();
    XrdCl::XRootDStatus status = self->process->Run( handler );
  }
}

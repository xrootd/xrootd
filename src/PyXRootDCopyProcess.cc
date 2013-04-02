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
    const char *source, *target;

    if ( !PyArg_ParseTuple( args, "ss", &source, &target ) ) return NULL;

    XrdCl::JobDescriptor *job = new XrdCl::JobDescriptor();
    job->source = XrdCl::URL( source );
    job->target = XrdCl::URL( target );

    self->process->AddJob( job );
    Py_RETURN_NONE;
  }

  PyObject* CopyProcess::Prepare( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    XrdCl::XRootDStatus status = self->process->Prepare();
    if (status.IsOK()) {
      printf(">>>>> 'prepped");
    }
  }

  PyObject* CopyProcess::Run( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    XrdCl::CopyProgressHandler *handler = new PyCopyProgressHandler();
    XrdCl::XRootDStatus status = self->process->Run( handler );
    if (status.IsOK()) {
      printf(">>>>> 'sall good");
    }
  }
}

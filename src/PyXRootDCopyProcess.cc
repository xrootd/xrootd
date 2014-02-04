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
#include "PyXRootDCopyProgressHandler.hh"
#include "Conversions.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  // Add a job to the copy process
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::AddJob( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]
      = { "source", "target", "sourcelimit", "force", "posc", "coerce",
          "thirdparty", "checksumprint", "chunksize", "parallelchunks", NULL };
    const char  *source;
    const char  *target;
    uint16_t sourceLimit   = 1;
    bool force             = false;
    bool posc              = false;
    bool coerce            = false;
    bool thirdParty        = false;
    bool checkSumPrint     = false;
    uint32_t chunkSize     = 4194304;
    uint8_t parallelChunks = 8;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HbbbbbIb:add_job",
         (char**) kwlist, &source, &target, &sourceLimit, &force, &posc,
         &coerce, &thirdParty, &checkSumPrint, &chunkSize, &parallelChunks ) )
      return NULL;

    XrdCl::JobDescriptor *job = new XrdCl::JobDescriptor();
    job->source = XrdCl::URL( source );
    job->target = XrdCl::URL( target );
    job->sourceLimit = sourceLimit;
    job->force = force;
    job->posc = posc;
    job->coerce = coerce;
    job->thirdParty = thirdParty;
    job->checkSumPrint = checkSumPrint;

    self->process->AddJob( job );
    Py_RETURN_NONE;
  }

  //----------------------------------------------------------------------------
  // Prepare the copy jobs
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::Prepare( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    XrdCl::XRootDStatus status = self->process->Prepare();
    return ConvertType<XrdCl::XRootDStatus>( &status );
  }

  //----------------------------------------------------------------------------
  // Run the copy jobs
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::Run( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    (void) CopyProcessType;   // Suppress unused variable warning
    static const char          *kwlist[]   = { "handler", NULL };
    PyObject                   *pyhandler  = 0;
    XrdCl::CopyProgressHandler *handler    = 0;

    if( !PyArg_ParseTupleAndKeywords( args, kwds, "|O", (char**) kwlist,
        &pyhandler ) ) return NULL;

    handler = new CopyProgressHandler( pyhandler );
    XrdCl::XRootDStatus status;

    //--------------------------------------------------------------------------
    //! Allow other threads to acquire the GIL while the copy jobs are running
    //--------------------------------------------------------------------------
    Py_BEGIN_ALLOW_THREADS
    status = self->process->Run( handler );
    Py_END_ALLOW_THREADS

    return ConvertType<XrdCl::XRootDStatus>( &status );
  }
}

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
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include "Conversions.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  // Add a job to the copy process
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::AddJob( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    //--------------------------------------------------------------------------
    // Initialize default parameters
    //--------------------------------------------------------------------------
    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

    static const char *kwlist[]
      = { "source", "target", "sourcelimit", "force", "posc", "coerce",
          "makedir", "thirdparty", "checksummode", "checksumtype",
          "checksumpreset", "chunksize", "parallelchunks", "inittimeout",
          "tpctimeout", "dynamicsource", 0 };

    const char  *source;
    const char  *target;
    uint16_t sourceLimit   = 1;
    bool force             = false;
    bool posc              = false;
    bool coerce            = false;
    bool makeDir           = false;
    const char *thirdParty       = "none";
    const char *checkSumMode     = "none";
    const char *checkSumType     = 0;
    const char *checkSumPreset   = 0;

    int tmp = XrdCl::DefaultCPChunkSize;
    env->GetInt( "CPChunkSize", tmp );
    uint32_t chunkSize     = tmp;
    tmp = XrdCl::DefaultCPParallelChunks;
    env->GetInt( "CPParallelChunks", tmp );
    uint8_t parallelChunks = tmp;
    tmp = XrdCl::DefaultCPInitTimeout;
    env->GetInt( "CPInitTimeout", tmp );
    uint16_t initTimeout   = tmp;
    tmp = XrdCl::DefaultCPTPCTimeout;
    env->GetInt( "CPTPCTimeout", tmp );
    uint16_t tpcTimeout    = tmp;

    bool     dynSrc = false;

    if( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HbbbbssssIBHHb:add_job",
         (char**) kwlist, &source, &target, &sourceLimit, &force, &posc,
         &coerce, &makeDir, &thirdParty, &checkSumMode, &checkSumType,
         &checkSumPreset, &chunkSize, &parallelChunks, &initTimeout,
         &tpcTimeout, &dynSrc ) )
      return NULL;

    //--------------------------------------------------------------------------
    // Add the job
    //--------------------------------------------------------------------------
    XrdCl::PropertyList  properties;
    XrdCl::PropertyList *results = new XrdCl::PropertyList;
    properties.Set( "source",         source         );
    properties.Set( "target",         target         );
    properties.Set( "force",          force          );
    properties.Set( "posc",           posc           );
    properties.Set( "coerce",         coerce         );
    properties.Set( "makeDir",        makeDir        );
    properties.Set( "dynamicSource",  dynSrc         );
    properties.Set( "thirdParty",     thirdParty     );
    properties.Set( "checkSumMode",   checkSumMode   );
    properties.Set( "checkSumType",   checkSumType   );
    properties.Set( "checkSumPreset", checkSumPreset );
    properties.Set( "chunkSize",      chunkSize      );
    properties.Set( "parallelChunks", (int)parallelChunks );

    //--------------------------------------------------------------------------
    // Add results
    //--------------------------------------------------------------------------
    XrdCl::XRootDStatus status = self->process->AddJob( properties, results );

    if( !status.IsOK() )
      delete results;

    self->results.push_back( results );

    return ConvertType<XrdCl::XRootDStatus>( &status );
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

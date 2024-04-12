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

#include "PyXRootDCopyProcess.hh"
#include "PyXRootDCopyProgressHandler.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include "Conversions.hh"
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClConstants.hh>

#include <memory>

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  // Set the number of parallel jobs
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::Parallel( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    static const char *kwlist[]
      = { "parallel", NULL };

    // we cannot submit a config job now because it needs to be the last one,
    // otherwise it will segv
    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "I:parallel",
         (char**) kwlist, &self->parallel ) )
      return NULL;

    XrdCl::XRootDStatus st;
    return ConvertType( &st );
  }

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
      = { "source", "target", "sourcelimit", "force", "posc",
          "coerce", "mkdir", "thirdparty", "checksummode", "checksumtype",
          "checksumpreset", "dynamicsource", "chunksize", "parallelchunks", "inittimeout",
          "tpctimeout", "rmBadCksum", "cptimeout", "xratethreshold", "xrate",
          "retry", "cont", "rtrplc", NULL };

    const char  *source;
    const char  *target;
    uint16_t     sourceLimit       = 1;
    bool         force             = false;
    bool         posc              = false;
    bool         coerce            = false;
    bool         mkdir             = false;
    const char  *thirdParty        = "none";
    const char  *checkSumMode      = "none";
    const char  *checkSumType      = "";
    const char  *checkSumPreset    = "";
    bool         dynamicSource     = false;
    bool         rmBadCksum        = false;
    long long    xRateThreshold    = 0;
    long long    xRate             = 0;
    long long    retry             = 0;
    bool         cont              = false;
    const char  *rtrplc            = "force";


    int val = XrdCl::DefaultCPChunkSize;
    env->GetInt( "CPChunkSize", val );
    uint32_t chunkSize = val;

    val = XrdCl::DefaultCPParallelChunks;
    env->GetInt( "CPParallelChunks", val );
    uint16_t parallelChunks = val;

    val = XrdCl::DefaultCPInitTimeout;
    env->GetInt( "CPInitTimeout", val );
    time_t initTimeout = val;

    val = XrdCl::DefaultCPTPCTimeout;
    env->GetInt( "CPTPCTimeout", val );
    time_t tpcTimeout = val;

    val = XrdCl::DefaultCPTimeout;
    env->GetInt( "CPTimeout", val );
    time_t cpTimeout = val;

    if ( !PyArg_ParseTupleAndKeywords( args, kwds, "ss|HbbbbssssbIHHHbHLLLbs:add_job",
         (char**) kwlist,
         &source, &target, &sourceLimit, &force, &posc,
         &coerce, &mkdir, &thirdParty, &checkSumMode, &checkSumType,
         &checkSumPreset, &dynamicSource, &chunkSize, &parallelChunks, &initTimeout,
         &tpcTimeout, &rmBadCksum, &cpTimeout, &xRateThreshold, &xRate,
         &retry, &cont, &rtrplc ) )
      return NULL;

    XrdCl::PropertyList properties;
    self->results->push_back(XrdCl::PropertyList());

    properties.Set( "source",          source          );
    properties.Set( "target",          target          );
    properties.Set( "force",           force           );
    properties.Set( "posc",            posc            );
    properties.Set( "coerce",          coerce          );
    properties.Set( "makeDir",         mkdir           );
    properties.Set( "dynamicSource",   dynamicSource   );
    properties.Set( "thirdParty",      thirdParty      );
    properties.Set( "checkSumMode",    checkSumMode    );
    properties.Set( "checkSumType",    checkSumType    );
    properties.Set( "checkSumPreset",  checkSumPreset  );
    properties.Set( "chunkSize",       chunkSize       );
    properties.Set( "parallelChunks",  parallelChunks  );
    properties.Set( "initTimeout",     initTimeout     );
    properties.Set( "tpcTimeout",      tpcTimeout      );
    properties.Set( "rmOnBadCksum",    rmBadCksum      );
    properties.Set( "cpTimeout",       cpTimeout       );
    properties.Set( "xrateThreshold",  xRateThreshold  );
    properties.Set( "xrate",           xRate           );
    properties.Set( "continue",        cont );

    env->PutInt( "CpRetry", retry );
    env->PutString( "CpRetryPolicy", rtrplc );

    if( sourceLimit > 1 )
    {
      int blockSize = XrdCl::DefaultXCpBlockSize;
      env->GetInt( "XCpBlockSize", blockSize );
      properties.Set( "xcp",          true        );
      properties.Set( "xcpBlockSize", blockSize   );
      properties.Set( "nbXcpSources", sourceLimit );
    }

    XrdCl::XRootDStatus status = self->process->AddJob(properties,
                                                       &self->results->back());

    return ConvertType( &status );
  }

  //----------------------------------------------------------------------------
  // Prepare the copy jobs
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::Prepare( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    // add a config job that sets the number of parallel copy jobs
    XrdCl::PropertyList processConfig;
    processConfig.Set( "jobType", "configuration" );
    processConfig.Set( "parallel", self->parallel );
    XrdCl::XRootDStatus status = self->process->AddJob(processConfig, 0);
    if( !status.IsOK() )
      return ConvertType( &status );

    status = self->process->Prepare();
    return ConvertType( &status );
  }

  //----------------------------------------------------------------------------
  // Run the copy jobs
  //----------------------------------------------------------------------------
  PyObject* CopyProcess::Run( CopyProcess *self, PyObject *args, PyObject *kwds )
  {
    (void) CopyProcessType;   // Suppress unused variable warning
    static const char          *kwlist[]   = { "handler", NULL };
    PyObject                   *pyhandler  = 0;
    std::unique_ptr<XrdCl::CopyProgressHandler> handler;

    if( !PyArg_ParseTupleAndKeywords( args, kwds, "|O", (char**) kwlist,
        &pyhandler ) ) return NULL;

    handler = std::make_unique<CopyProgressHandler>( pyhandler );
    XrdCl::XRootDStatus status;

    //--------------------------------------------------------------------------
    //! Allow other threads to acquire the GIL while the copy jobs are running
    //--------------------------------------------------------------------------
    Py_BEGIN_ALLOW_THREADS
    status = self->process->Run( handler.get() );
    Py_END_ALLOW_THREADS

    PyObject *tuple = PyTuple_New(2);
    PyTuple_SetItem(tuple, 0, ConvertType(&status));
    PyTuple_SetItem(tuple, 1, ConvertType(self->results));

    return tuple;
  }
}

//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClClassicCopyJob.hh"
#include "XrdCl/XrdClTPFallBackCopyJob.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClCopyJob.hh"
#include "XrdCl/XrdClUtils.hh"

#include <sys/time.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  CopyProcess::~CopyProcess()
  {
    CleanUpJobs();
  }

  //----------------------------------------------------------------------------
  // Add job
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::AddJob( const PropertyList &properties,
                                    PropertyList       *results )
  {
    Env *env = DefaultEnv::GetEnv();

    //--------------------------------------------------------------------------
    // Validate properties
    //--------------------------------------------------------------------------
    if( !properties.HasProperty( "source" ) )
      return XRootDStatus( stError, errInvalidArgs, 0, "source not specified" );

    if( !properties.HasProperty( "target" ) )
      return XRootDStatus( stError, errInvalidArgs, 0, "target not specified" );

    pJobProperties.push_back( properties );
    PropertyList &p = pJobProperties.back();

    const char *bools[] = {"target", "force", "posc", "coerce", "makeDir", 0};
    for( int i = 0; bools[i]; ++i )
      if( !p.HasProperty( bools[i] ) )
        p.Set( bools[i], false );

    if( !p.HasProperty( "thirdParty" ) )
      p.Set( "thirdParty", "none" );

    if( !p.HasProperty( "checkSumMode" ) )
      p.Set( "checkSumMode", "none" );
    else
    {
      if( !p.HasProperty( "checkSumType" ) )
      {
        pJobProperties.pop_back();
        return XRootDStatus( stError, errInvalidArgs, 0,
                             "checkSumType not specified" );
      }
    }

    if( !p.HasProperty( "parallelChunks" ) )
    {
      int val = DefaultCPParallelChunks;
      env->GetInt( "CPParallelChunks", val );
      p.Set( "parallelChunks", val );
    }

    if( !p.HasProperty( "chunkSize" ) )
    {
      int val = DefaultCPChunkSize;
      env->GetInt( "CPChunkSize", val );
      p.Set( "chunkSize", val );
    }

    if( !p.HasProperty( "initTimeout" ) )
    {
      int val = DefaultCPInitTimeout;
      env->GetInt( "CPInitTimeout", val );
      p.Set( "initTimeout", val );
    }

    if( !p.HasProperty( "tpcTimeout" ) )
    {
      int val = DefaultCPTPCTimeout;
      env->GetInt( "CPTPCTimeout", val );
      p.Set( "tpcTimeout", val );
    }

    if( !p.HasProperty( "dynamicSource" ) )
      p.Set( "dynamicSource", false );

    //--------------------------------------------------------------------------
    // Insert the properties
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    Utils::LogPropertyList( log, UtilityMsg, "Adding job with properties: %s",
                            p );
    pJobResults.push_back( results );
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Prepare the copy jobs
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::Prepare()
  {
    Log *log = DefaultEnv::GetLog();
    std::vector<PropertyList>::iterator it;

    log->Debug( UtilityMsg, "CopyProcess: %d jobs to prepare",
                pJobProperties.size() );

    std::map<std::string, uint32_t> targetFlags;
    int i = 0;
    for( it = pJobProperties.begin(); it != pJobProperties.end(); ++it, ++i )
    {
      PropertyList &props = *it;
      PropertyList *res   = pJobResults[i];
      std::string tmp;

      props.Get( "source", tmp );
      URL source = tmp;
      if( !source.IsValid() )
        return XRootDStatus( stError, errInvalidArgs, 0, "invalid source" );

      props.Get( "target", tmp );
      URL target = tmp;
      if( !target.IsValid() )
        return XRootDStatus( stError, errInvalidArgs, 0, "invalid target" );

      bool tpc = false;
      props.Get( "thirdParty", tmp );
      if( tmp != "none" )
        tpc = true;

      //------------------------------------------------------------------------
      // Check if we have all we need
      //------------------------------------------------------------------------
      if( source.GetProtocol() != "stdio" && source.GetPath().empty() )
      {
        log->Debug( UtilityMsg, "CopyProcess (job #%d): no source specified.",
                    i );
        CleanUpJobs();
        XRootDStatus st = XRootDStatus( stError, errInvalidArgs );
        res->Set( "status", st );
        return st;
      }

      if( target.GetProtocol() != "stdio" && target.GetPath().empty() )
      {
        log->Debug( UtilityMsg, "CopyProcess (job #%d): no target specified.",
                    i );
        CleanUpJobs();
        XRootDStatus st = XRootDStatus( stError, errInvalidArgs );
        res->Set( "status", st );
        return st;
      }

      //------------------------------------------------------------------------
      // Check what kind of job we should do
      //------------------------------------------------------------------------
      CopyJob *job = 0;

      if( tpc == true )
        job = new TPFallBackCopyJob( i+1, &props, res );
      else
        job = new ClassicCopyJob( i+1, &props, res );

      pJobs.push_back( job );
    }
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Run the copy jobs
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::Run( CopyProgressHandler *progress )
  {
    std::vector<CopyJob *>::iterator it;
    uint16_t currentJob = 1;
    uint16_t totalJobs  = pJobs.size();

    Monitor *mon = DefaultEnv::GetMonitor();
    timeval bTOD;

    for( it = pJobs.begin(); it != pJobs.end(); ++it )
    {
      //------------------------------------------------------------------------
      // Report beginning of the copy
      //------------------------------------------------------------------------
      if( progress )
        progress->BeginJob( currentJob, totalJobs, &(*it)->GetSource(),
                            &(*it)->GetTarget() );

      if( mon )
      {
        Monitor::CopyBInfo i;
        i.transfer.origin = &(*it)->GetSource();
        i.transfer.target = &(*it)->GetTarget();
        mon->Event( Monitor::EvCopyBeg, &i );
      }

      gettimeofday( &bTOD, 0 );

      //------------------------------------------------------------------------
      // Do the copy
      //------------------------------------------------------------------------
      XRootDStatus st = (*it)->Run( progress );
      (*it)->GetResults()->Set( "status", st );

      //------------------------------------------------------------------------
      // Report end of the copy
      //------------------------------------------------------------------------
      if( mon )
      {
        std::vector<std::string> sources;
        (*it)->GetResults()->Get( "sources", sources );
        Monitor::CopyEInfo i;
        i.transfer.origin = &(*it)->GetSource();
        i.transfer.target = &(*it)->GetTarget();
        i.sources         = sources.size();
        i.bTOD            = bTOD;
        gettimeofday( &i.eTOD, 0 );
        i.status          = &st;
        mon->Event( Monitor::EvCopyEnd, &i );
      }

      if( progress )
        progress->EndJob( currentJob, (*it)->GetResults() );
      if( !st.IsOK() ) return st;
      ++currentJob;
    }
    return XRootDStatus();
  }

  void CopyProcess::CleanUpJobs()
  {
    std::vector<CopyJob*>::iterator itJ;
    for( itJ = pJobs.begin(); itJ != pJobs.end(); ++itJ )
      delete *itJ;
    pJobs.clear();
  }
}

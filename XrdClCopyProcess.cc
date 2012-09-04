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
#include "XrdCl/XrdClThirdPartyCopyJob.hh"
#include "XrdCl/XrdClFileSystem.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  CopyProcess::~CopyProcess()
  {
    delete pDestination;
    std::list<URL*>::iterator it;
    for( it = pSource.begin(); it != pSource.end(); ++it )
      delete *it;
    for( it = pDestinations.begin(); it != pDestinations.end(); ++it )
      delete *it;
  }

  //----------------------------------------------------------------------------
  // Add source
  //----------------------------------------------------------------------------
  bool CopyProcess::AddSource( const std::string &source )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "CopyProcess: adding source: %s", source.c_str() );
    URL *url = new URL( source );

    if( !url->IsValid() )
    {
      log->Debug( UtilityMsg, "CopyProcess: source is invalid: %s",
                              source.c_str() );
      delete url;
      return false;
    }
    pSource.push_back( url );
    return true;
  }

  //----------------------------------------------------------------------------
  // Add source
  //----------------------------------------------------------------------------
  bool CopyProcess::AddSource( const URL &source )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "CopyProcess: adding source: %s",
                            source.GetURL().c_str() );
    if( !source.IsValid() )
    {
      log->Debug( UtilityMsg, "CopyProcess: source is invalid: %s",
                              source.GetURL().c_str() );
      return false;
    }
    pSource.push_back( new URL( source ) );
    return true;
  }

  //----------------------------------------------------------------------------
  // Set destination
  //----------------------------------------------------------------------------
  bool CopyProcess::SetDestination( const std::string &destination )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "CopyProcess: adding destination: %s",
                            destination.c_str() );
    URL *url = new URL( destination );
    if( !url->IsValid() )
    {
      log->Debug( UtilityMsg, "CopyProcess: destination is invalid: %s",
                              destination.c_str() );
      delete url;
      return false;
    }
    pDestination = url;
    return true;
  }

  //----------------------------------------------------------------------------
  // Set destination
  //----------------------------------------------------------------------------
  bool CopyProcess::SetDestination( const URL &destination )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "CopyProcess: adding destination: %s",
                            destination.GetURL().c_str() );

    if( !destination.IsValid() )
    {
      log->Debug( UtilityMsg, "CopyProcess: destination is invalid: %s",
                              destination.GetURL().c_str() );
      return false;
    }
    pDestination = new URL( destination );
    return true;
  }

  //----------------------------------------------------------------------------
  // Prepare the copy jobs
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::Prepare()
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Check if we have all we need
    //--------------------------------------------------------------------------
    if( !pDestination || pSource.empty() )
    {
      log->Debug( UtilityMsg, "CopyProcess: no source or destination "
                              "specified." );
      return Status( stError, errInvalidArgs );
    }

    //--------------------------------------------------------------------------
    // We just have one job to do
    //--------------------------------------------------------------------------
    if( pSource.size() == 1 )
    {
      CopyJob *job = 0;
      if( pThirdParty )
        job = new ThirdPartyCopyJob( pSource.front(), pDestination );
      else
        job = new ClassicCopyJob( pSource.front(), pDestination );
      pJobs.push_back( job );
      job->SetForce( pForce );
      job->SetPosc( pPosc );
    }
    //--------------------------------------------------------------------------
    // Many jobs
    //--------------------------------------------------------------------------
    else
    {
      //------------------------------------------------------------------------
      // Check if the remote path exist and is a directory
      //------------------------------------------------------------------------
      FileSystem fs( *pDestination );
      StatInfo *statInfo = 0;
      XRootDStatus st = fs.Stat( pDestination->GetPath(), statInfo );
      if( !st.IsOK() )
        return st;

      if( !statInfo->TestFlags( StatInfo::IsDir ) )
      {
        delete statInfo;
        log->Debug( UtilityMsg, "CopyProcess: destination for recursive copy "
                                "is not a directory." );

        return Status( stError, errInvalidArgs, EINVAL );
      }

      //------------------------------------------------------------------------
      // Loop through the sources and create the destination paths
      //------------------------------------------------------------------------
      std::list<URL*>::iterator it;
      for( it = pSource.begin(); it != pSource.end(); ++it )
      {
        std::string pathSuffix = (*it)->GetPath();
        pathSuffix = pathSuffix.substr( pRootOffset,
                                        pathSuffix.length()-pRootOffset );
        URL *dst = new URL( *pDestination );
        dst->SetPath( dst->GetPath() + pathSuffix );
        pDestinations.push_back( dst );

        CopyJob *job = 0;
        if( pThirdParty )
          job = new ThirdPartyCopyJob( pSource.front(), pDestination );
        else
          job = new ClassicCopyJob( *it, dst );
        pJobs.push_back( job );
        job->SetForce( pForce );
        job->SetPosc( pPosc );
      }
    }

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Run the copy jobs
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::Run()
  {
    std::list<CopyJob *>::iterator it;
    uint16_t currentJob = 1;
    uint16_t totalJobs  = pJobs.size();
    for( it = pJobs.begin(); it != pJobs.end(); ++it )
    {
      if( pProgressHandler )
        pProgressHandler->BeginJob( currentJob, totalJobs,
                                    (*it)->GetSource(),
                                    (*it)->GetDestination() );
      XRootDStatus st = (*it)->Run( pProgressHandler );
      if( pProgressHandler )
        pProgressHandler->EndJob( st );
      if( !st.IsOK() ) return st;
      ++currentJob;
    }
    return XRootDStatus();
  }
}

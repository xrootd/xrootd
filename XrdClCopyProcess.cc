//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClClassicCopyJob.hh"

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
    // Build the jobs
    //--------------------------------------------------------------------------
    CopyJob *job = new ClassicCopyJob( pSource.front(), pDestination );
    pJobs.push_back( job );

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
      pProgressHandler->BeginJob( currentJob, totalJobs,
                                  (*it)->GetSource(),
                                  (*it)->GetDestination() );
      XRootDStatus st = (*it)->Run( pProgressHandler );
      pProgressHandler->EndJob( st );
      if( !st.IsOK() ) return st;
    }
    return XRootDStatus();
  }
}

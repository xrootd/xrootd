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
#include "XrdCl/XrdClMonitor.hh"

#include <sys/time.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  CopyProcess::~CopyProcess()
  {
    std::list<CopyJob*>::iterator itJ;
    for( itJ = pJobs.begin(); itJ != pJobs.end(); ++itJ )
      delete *itJ;
  }

  //----------------------------------------------------------------------------
  // Prepare the copy jobs
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::Prepare()
  {
    Log *log = DefaultEnv::GetLog();
    std::list<JobDescriptor*>::iterator it;

    log->Debug( UtilityMsg, "CopyProcess: %d jobs to prepare",
                pJobDescs.size() );

    std::map<std::string, uint32_t> targetFlags;
    int i = 0;
    for( it = pJobDescs.begin(); it != pJobDescs.end(); ++it, ++i )
    {
      JobDescriptor *jobDesc = *it;

      //------------------------------------------------------------------------
      // Check if we have all we need
      //------------------------------------------------------------------------
      if( jobDesc->source.GetPath().empty() ||
          jobDesc->target.GetPath().empty() )
      {
        log->Debug( UtilityMsg, "CopyProcess (job #%d): no source or "
                    "destination specified.", i );
        return Status( stError, errInvalidArgs );
      }

      CopyJob *job = 0;
      if( jobDesc->thirdParty )
        job = new ThirdPartyCopyJob( jobDesc );
      else
        job = new ClassicCopyJob( jobDesc );
      pJobs.push_back( job );
    }
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Run the copy jobs
  //----------------------------------------------------------------------------
  XRootDStatus CopyProcess::Run( CopyProgressHandler *progress )
  {
    std::list<CopyJob *>::iterator it;
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
        progress->BeginJob( currentJob, totalJobs,
                            &(*it)->GetDescriptor()->source,
                            &(*it)->GetDescriptor()->target );

      if( mon )
      {
        Monitor::CopyBInfo i;
        i.transfer.origin = &(*it)->GetDescriptor()->source;
        i.transfer.target = &(*it)->GetDescriptor()->target;
        mon->Event( Monitor::EvCopyBeg, &i );
      }

      gettimeofday( &bTOD, 0 );

      //------------------------------------------------------------------------
      // Do the copy
      //------------------------------------------------------------------------
      XRootDStatus st = (*it)->Run( progress );

      //------------------------------------------------------------------------
      // Report end of the copy
      //------------------------------------------------------------------------
      if( mon )
      {
        Monitor::CopyEInfo i;
        i.transfer.origin = &(*it)->GetDescriptor()->source;
        i.transfer.target = &(*it)->GetDescriptor()->target;
        i.sources         = (*it)->GetDescriptor()->sources.size();
        i.bTOD            = bTOD;
        gettimeofday( &i.eTOD, 0 );
        i.status          = &st;
        mon->Event( Monitor::EvCopyEnd, &i );
      }

      if( progress )
        progress->EndJob( st );
      if( !st.IsOK() ) return st;
      ++currentJob;
    }
    return XRootDStatus();
  }
}

//------------------------------------------------------------------------------
// Copyright (c) 2014 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClTPFallBackCopyJob.hh"
#include "XrdCl/XrdClThirdPartyCopyJob.hh"
#include "XrdCl/XrdClClassicCopyJob.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include <optional>
#include <string>

using namespace std::string_literals;

namespace XrdCl
{
  namespace
  {
    //--------------------------------------------------------------------------
    //! Forwards the progress of a plug-in third party copy to the copy job
    //! progress handler.
    //!
    //! The plug-in reports the total size only when it knows it. This handler
    //! keeps the last reported total for the notifications which follow.
    //--------------------------------------------------------------------------
    class PlugInProgressHandler final : public ProgressHandler
    {
      public:
        PlugInProgressHandler( uint32_t             job_id,
                               CopyProgressHandler &progress ):
          progress( progress ),
          job_id( job_id )
        {
        }

        void HandleProgress( std::size_t processed,
                             std::size_t total ) override
        {
          if( total )
            this->total = total;

          progress.JobProgress( job_id, processed, this->total );
        }

      private:
        CopyProgressHandler &progress;
        const uint32_t       job_id;
        std::size_t          total = 0;
    };
  }

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  TPFallBackCopyJob::TPFallBackCopyJob( uint32_t      jobId,
                                        PropertyList *jobProperties,
                                        PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a third party fall back copy job, "
                "from %s to %s", GetSource().GetObfuscatedURL().c_str(),
                GetTarget().GetObfuscatedURL().c_str() );
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus TPFallBackCopyJob::Run( CopyProgressHandler *progress )
  {
    //--------------------------------------------------------------------------
    // Set up the job
    //--------------------------------------------------------------------------
    const bool tpcFallBack = pProperties->Get<std::string>( "thirdParty" ) == "first"s;

    XRootDStatus st = ThirdPartyCopyJob(pJobId, pProperties, pResults).Run(progress);
    if( st.IsOK() ) return st; // we are done

    // try with the plugins
    if (st.code == errNotSupported)
    {
      FileSystem fs(GetSource());

      std::optional<PlugInProgressHandler> progress_handler;
      if( progress )
        progress_handler.emplace( pJobId, *progress );

      st = fs.ThirdPartyCopy(GetSource().GetURL(), GetTarget().GetURL(), pProperties,
                             progress_handler ? &progress_handler.value() : nullptr);
      if (st.IsOK())
        return st;
    }

    // check if we can fall back to streaming
    if( tpcFallBack && ( st.code == errNotSupported || st.code == errOperationExpired ) )
    {
      Log *log = DefaultEnv::GetLog();
      log->Debug( UtilityMsg, "TPC is not supported, falling back to streaming mode." );

      return ClassicCopyJob(pJobId, pProperties, pResults).Run(progress);
    }

    return st;
  }
}

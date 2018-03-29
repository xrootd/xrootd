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
#include <string>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  TPFallBackCopyJob::TPFallBackCopyJob( uint16_t      jobId,
                                        PropertyList *jobProperties,
                                        PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults ),
    pJob( 0 )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a third party fall back copy job, "
                "from %s to %s", GetSource().GetURL().c_str(),
                GetTarget().GetURL().c_str() );
  }

  //------------------------------------------------------------------------
  // Destructor
  //------------------------------------------------------------------------
  TPFallBackCopyJob::~TPFallBackCopyJob()
  {
    delete pJob;
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus TPFallBackCopyJob::Run( CopyProgressHandler *progress )
  {
    //--------------------------------------------------------------------------
    // Set up the job
    //--------------------------------------------------------------------------
    std::string  tmp;
    bool         tpcFallBack = false;

    pProperties->Get( "thirdParty", tmp );
    if( tmp == "first" )
      tpcFallBack = true;

    XRootDStatus st = ThirdPartyCopyJob::CanDo( GetSource(), GetTarget(),
                                                pProperties );

    if( st.IsOK() )
      pJob = new ThirdPartyCopyJob( pJobId, pProperties, pResults );
    else if( tpcFallBack && !st.IsFatal() )
      pJob = new ClassicCopyJob( pJobId, pProperties, pResults );
    else
      return st;

    //--------------------------------------------------------------------------
    // Run the job
    //--------------------------------------------------------------------------
    return pJob->Run( progress );
  }
}

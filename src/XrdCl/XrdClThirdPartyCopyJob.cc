//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClThirdPartyCopyJob.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClUglyHacks.hh"
#include "XrdOuc/XrdOucTPC.hh"
#include "XrdSys/XrdSysTimer.hh"
#include <iostream>
#include <cctype>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{
  //----------------------------------------------------------------------------
  //! Handle an async response
  //----------------------------------------------------------------------------
  class TPCStatusHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      TPCStatusHandler():
        pSem( new XrdCl::Semaphore(0) ), pStatus(0)
      {
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~TPCStatusHandler()
      {
        delete pStatus;
        delete pSem;
      }

      //------------------------------------------------------------------------
      // Handle Response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        delete response;
        pStatus = status;
        pSem->Post();
      }

      //------------------------------------------------------------------------
      // Get Mutex
      //------------------------------------------------------------------------
      XrdCl::Semaphore *GetSemaphore()
      {
        return pSem;
      }

      //------------------------------------------------------------------------
      // Get status
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus *GetStatus()
      {
        return pStatus;
      }

    private:
      TPCStatusHandler(const TPCStatusHandler &other);
      TPCStatusHandler &operator = (const TPCStatusHandler &other);

      XrdCl::Semaphore    *pSem;
      XrdCl::XRootDStatus *pStatus;
  };

}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  ThirdPartyCopyJob::ThirdPartyCopyJob( uint16_t      jobId,
                                        PropertyList *jobProperties,
                                        PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a third party copy job, from %s to %s",
                GetSource().GetURL().c_str(), GetTarget().GetURL().c_str() );
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopyJob::Run( CopyProgressHandler *progress )
  {
    //--------------------------------------------------------------------------
    // Decode the parameters
    //--------------------------------------------------------------------------
    std::string checkSumMode;
    std::string checkSumType;
    std::string checkSumPreset;
    uint64_t    sourceSize;
    bool        force, coerce;

    pProperties->Get( "checkSumMode",    checkSumMode );
    pProperties->Get( "checkSumType",    checkSumType );
    pProperties->Get( "checkSumPreset",  checkSumPreset );
    pProperties->Get( "sourceSize",      sourceSize );
    pProperties->Get( "force",           force );
    pProperties->Get( "coerce",          coerce );

    //--------------------------------------------------------------------------
    // Generate the destination CGI
    //--------------------------------------------------------------------------
    URL tpcSource;
    pProperties->Get( "tpcSource", tpcSource );

    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Generating the TPC URLs" );

    std::string  tpcKey = GenerateKey();
    char        *cgiBuff = new char[2048];
    const char  *cgiP = XrdOucTPC::cgiC2Dst( tpcKey.c_str(),
                                             tpcSource.GetHostId().c_str(),
                                             tpcSource.GetPath().c_str(),
                                             0, cgiBuff, 2048 );
    if( *cgiP == '!' )
    {
      log->Error( UtilityMsg, "Unable to setup target url: %s", cgiP+1 );
      delete [] cgiBuff;
      return XRootDStatus( stError, errInvalidArgs );
    }

    URL cgiURL; cgiURL.SetParams( cgiBuff );
    delete [] cgiBuff;

    URL realTarget = GetTarget();
    URL::ParamsMap params = realTarget.GetParams();
    MessageUtils::MergeCGI( params, cgiURL.GetParams(), true );

    std::ostringstream o; o << sourceSize;
    params["oss.asize"] = o.str();
    params["tpc.stage"] = "copy";
    realTarget.SetParams( params );

    log->Debug( UtilityMsg, "Target url is: %s", realTarget.GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Timeouts
    //--------------------------------------------------------------------------
    uint16_t timeLeft = 0;
    pProperties->Get( "initTimeout", timeLeft );

    time_t   start          = 0;
    bool     hasInitTimeout = false;

    if( timeLeft )
    {
      hasInitTimeout = true;
      start          = time(0);
    }

    uint16_t tpcTimeout = 0;
    pProperties->Get( "tpcTimeout", tpcTimeout );

    //--------------------------------------------------------------------------
    // Open the target file
    //--------------------------------------------------------------------------
    File targetFile;
    OpenFlags::Flags targetFlags = OpenFlags::Update;
    if( force )
      targetFlags |= OpenFlags::Delete;
    else
      targetFlags |= OpenFlags::New;

    if( coerce )
      targetFlags |= OpenFlags::Force;

    XRootDStatus st;
    st = targetFile.Open( realTarget.GetURL(), targetFlags, Access::None,
                          timeLeft );

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable to open target %s: %s",
                  realTarget.GetURL().c_str(), st.ToStr().c_str() );
      return st;
    }
    std::string lastUrl; targetFile.GetProperty( "LastURL", lastUrl );
    realTarget = lastUrl;

    //--------------------------------------------------------------------------
    // Generate the source CGI
    //--------------------------------------------------------------------------
    cgiBuff = new char[2048];
    cgiP = XrdOucTPC::cgiC2Src( tpcKey.c_str(),
                                realTarget.GetHostName().c_str(), -1, cgiBuff,
                                2048 );
    if( *cgiP == '!' )
    {
      log->Error( UtilityMsg, "Unable to setup source url: %s", cgiP+1 );
      delete [] cgiBuff;
      return XRootDStatus( stError, errInvalidArgs );
    }

    cgiURL.SetParams( cgiBuff );
    delete [] cgiBuff;
    params = tpcSource.GetParams();
    MessageUtils::MergeCGI( params, cgiURL.GetParams(), true );
    params["tpc.stage"] = "copy";
    tpcSource.SetParams( params );

    log->Debug( UtilityMsg, "Source url is: %s", tpcSource.GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Calculate the time we hav left to perform source open
    //--------------------------------------------------------------------------
    if( hasInitTimeout )
    {
      time_t now = time(0);
      if( now-start > timeLeft )
      {
        targetFile.Close(1);
        return XRootDStatus( stError, errOperationExpired );
      }
      else
        timeLeft -= (now-start);
    }

    //--------------------------------------------------------------------------
    // Set up the rendez-vous and open the source
    //--------------------------------------------------------------------------
    st = targetFile.Sync( tpcTimeout );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable set up rendez-vous: %s",
                   st.ToStr().c_str() );
      targetFile.Close();
      return st;
    }

    File sourceFile;
    st = sourceFile.Open( tpcSource.GetURL(), OpenFlags::Read, Access::None,
                          timeLeft );

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable to open source %s: %s",
                  tpcSource.GetURL().c_str(), st.ToStr().c_str() );
      targetFile.Close(1);
      return st;
    }

    //--------------------------------------------------------------------------
    // Do the copy and follow progress
    //--------------------------------------------------------------------------
    TPCStatusHandler  statusHandler;
    Semaphore        *sem  = statusHandler.GetSemaphore();
    StatInfo         *info   = 0;

    st = targetFile.Sync( &statusHandler );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable start the copy: %s",
                  st.ToStr().c_str() );
      sourceFile.Close();
      targetFile.Close();
      return st;
    }

    //--------------------------------------------------------------------------
    // Stat the file every second until sync returns
    //--------------------------------------------------------------------------
    bool canceled = false;
    while( 1 )
    {
      XrdSysTimer::Wait( 1000 );

      if( progress )
      {
        st = targetFile.Stat( true, info );
        if( st.IsOK() )
        {
          progress->JobProgress( pJobId, info->GetSize(), sourceSize );
          delete info;
          info = 0;
        }
        bool shouldCancel = progress->ShouldCancel( pJobId );
        if( shouldCancel )
        {
          log->Debug( UtilityMsg, "Cancelation requested by progress handler" );
          Buffer arg, *response = 0; arg.FromString( "ofs.tpc cancel" );
          XRootDStatus st = targetFile.Fcntl( arg, response );
          if( !st.IsOK() )
            log->Debug( UtilityMsg, "Error while trying to cancel tpc: %s",
                        st.ToStr().c_str() );

          delete response;
          canceled = true;
          break;
        }
      }

      if( sem->CondWait() )
        break;
    }

    //--------------------------------------------------------------------------
    // Sync has returned so we can check if it was successful
    //--------------------------------------------------------------------------
    if( canceled )
      sem->Wait();

    st = *statusHandler.GetStatus();

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Third party copy from %s to %s failed: %s",
                  GetSource().GetURL().c_str(), GetTarget().GetURL().c_str(),
                  st.ToStr().c_str() );

      sourceFile.Close(1);
      targetFile.Close(1);
      return st;
    }

    log->Debug( UtilityMsg, "Third party copy from %s to %s successful",
                GetSource().GetURL().c_str(), GetTarget().GetURL().c_str() );

    sourceFile.Close(1);
    targetFile.Close(1);

    pResults->Set( "size", sourceSize );

    //--------------------------------------------------------------------------
    // Verify the checksums if needed
    //--------------------------------------------------------------------------
    if( checkSumMode != "none" )
    {
      log->Debug( UtilityMsg, "Attempting checksum calculation." );
      std::string sourceCheckSum;
      std::string targetCheckSum;

      //------------------------------------------------------------------------
      // Get the check sum at source
      //------------------------------------------------------------------------
      timeval oStart, oEnd;
      XRootDStatus st;
      if( checkSumMode == "end2end" || checkSumMode == "source" )
      {
        gettimeofday( &oStart, 0 );
        if( !checkSumPreset.empty() )
        {
          sourceCheckSum  = checkSumType + ":";
          sourceCheckSum += checkSumPreset;
        }
        else
        {
          st = Utils::GetRemoteCheckSum( sourceCheckSum, checkSumType,
                                         GetSource().GetHostId(),
                                         GetSource().GetPath() );
        }
        gettimeofday( &oEnd, 0 );
        if( !st.IsOK() )
          return st;

        pResults->Set( "sourceCheckSum", sourceCheckSum );
      }

      //------------------------------------------------------------------------
      // Get the check sum at destination
      //------------------------------------------------------------------------
      timeval tStart, tEnd;

      if( checkSumMode == "end2end" || checkSumMode == "target" )
      {
        gettimeofday( &tStart, 0 );
        st = Utils::GetRemoteCheckSum( targetCheckSum, checkSumType,
                                       GetTarget().GetHostId(),
                                       GetTarget().GetPath() );

        gettimeofday( &tEnd, 0 );
        if( !st.IsOK() )
          return st;
        pResults->Set( "targetCheckSum", targetCheckSum );
      }

      //------------------------------------------------------------------------
      // Compare and inform monitoring
      //------------------------------------------------------------------------
      if( checkSumMode == "end2end" )
      {
        bool match = false;
        if( sourceCheckSum == targetCheckSum )
          match = true;

        Monitor *mon = DefaultEnv::GetMonitor();
        if( mon )
        {
          Monitor::CheckSumInfo i;
          i.transfer.origin = &GetSource();
          i.transfer.target = &GetTarget();
          i.cksum           = sourceCheckSum;
          i.oTime           = Utils::GetElapsedMicroSecs( oStart, oEnd );
          i.tTime           = Utils::GetElapsedMicroSecs( tStart, tEnd );
          i.isOK            = match;
          mon->Event( Monitor::EvCheckSum, &i );
        }

        if( !match )
          return XRootDStatus( stError, errCheckSumError, 0 );
      }
    }
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Check whether doing a third party copy is feasible for given
  // job descriptor
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopyJob::CanDo( const URL &source, const URL &target,
                                         PropertyList *properties )
  {

    //--------------------------------------------------------------------------
    // Check the initial settings
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Check if third party copy between %s and %s "
                "is possible", source.GetURL().c_str(),
                target.GetURL().c_str() );


    if( source.GetProtocol() != "root" &&
        source.GetProtocol() != "xroot" )
      return XRootDStatus( stError, errNotSupported );

    if( target.GetProtocol() != "root" &&
        target.GetProtocol() != "xroot" )
      return XRootDStatus( stError, errNotSupported );

    uint16_t timeLeft       = 0;
    properties->Get( "initTimeout", timeLeft );

    time_t   start          = 0;
    bool     hasInitTimeout = false;

    if( timeLeft )
    {
      hasInitTimeout = true;
      start          = time(0);
    }

    //--------------------------------------------------------------------------
    // Check if we can open the source file and whether the actual data server
    // can support the third party copy
    //--------------------------------------------------------------------------
    File          sourceFile;
    XRootDStatus  st;
    URL           sourceURL = source;

    URL::ParamsMap params = sourceURL.GetParams();
    params["tpc.stage"] = "placement";
    sourceURL.SetParams( params );
    log->Debug( UtilityMsg, "Trying to open %s for reading",
                sourceURL.GetURL().c_str() );
    st = sourceFile.Open( sourceURL.GetURL(), OpenFlags::Read, Access::None,
                          timeLeft );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Cannot open source file %s: %s",
                  source.GetURL().c_str(), st.ToStr().c_str() );
      st.status = stFatal;
      return st;
    }
    std::string sourceUrl; sourceFile.GetProperty( "LastURL", sourceUrl );
    URL         sourceUrlU = sourceUrl;
    properties->Set( "tpcSource", sourceUrl );
    StatInfo *statInfo;
    sourceFile.Stat( false, statInfo );
    properties->Set( "sourceSize", statInfo->GetSize() );
    delete statInfo;

    if( hasInitTimeout )
    {
      time_t now = time(0);
      if( now-start > timeLeft )
        timeLeft = 1; // we still want to send a close, but we time it out fast
      else
        timeLeft -= (now-start);
    }

    sourceFile.Close( timeLeft );

    if( hasInitTimeout )
    {
      time_t now = time(0);
      if( now-start > timeLeft )
        return XRootDStatus( stError, errOperationExpired );
      else
        timeLeft -= (now-start);
    }

    st = Utils::CheckTPC( sourceUrlU.GetHostId(), timeLeft );
    if( !st.IsOK() )
      return st;

    //--------------------------------------------------------------------------
    // Verify the destination
    //--------------------------------------------------------------------------
//    st = Utils::CheckTPC( jd->target.GetHostId() );
//    if( !st.IsOK() )
//      return st;

    if( hasInitTimeout )
    {
      if( timeLeft == 0 )
        properties->Set( "initTimeout", 1 );
      else
        properties->Set( "initTimeout", timeLeft );
    }
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Generate a rendez-vous key
  //----------------------------------------------------------------------------
  std::string ThirdPartyCopyJob::GenerateKey()
  {
    char tpcKey[25];
    struct timeval  currentTime;
    struct timezone tz;
    gettimeofday( &currentTime, &tz );
    int k1 = currentTime.tv_usec;
    int k2 = getpid() | (getppid() << 16);
    int k3 = currentTime.tv_sec;
    snprintf( tpcKey, 25, "%08x%08x%08x", k1, k2, k3 );
    return std::string(tpcKey);
  }
}

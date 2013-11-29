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
      XrdCl::Semaphore    *pSem;
      XrdCl::XRootDStatus *pStatus;
  };

}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  ThirdPartyCopyJob::ThirdPartyCopyJob( JobDescriptor *jobDesc,
                                        Info          *tpcInfo ):
    CopyJob( jobDesc ), pTPCInfo( *tpcInfo )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a third party copy job, from %s to %s",
                pJob->source.GetURL().c_str(),
                pJob->target.GetURL().c_str() );
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopyJob::Run( CopyProgressHandler *progress )
  {
    //--------------------------------------------------------------------------
    // Generate the destination CGI
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Generating the TPC URLs" );

    std::string  tpcKey = GenerateKey();
    char        *cgiBuff = new char[2048];
    const char  *cgiP = XrdOucTPC::cgiC2Dst( tpcKey.c_str(),
                                             pTPCInfo.source.GetHostId().c_str(),
                                             pJob->source.GetPath().c_str(),
                                             0, cgiBuff, 2048 );
    if( *cgiP == '!' )
    {
      log->Error( UtilityMsg, "Unable to setup target url: %s", cgiP+1 );
      delete [] cgiBuff;
      return XRootDStatus( stError, errInvalidArgs );
    }

    URL cgiURL; cgiURL.SetParams( cgiBuff );
    delete [] cgiBuff;

    pJob->realTarget = pJob->target;
    URL::ParamsMap params = pJob->realTarget.GetParams();
    MessageUtils::MergeCGI( params, cgiURL.GetParams(), true );

    std::ostringstream o; o << pTPCInfo.sourceSize;
    params["oss.asize"] = o.str();
    params["tpc.stage"] = "copy";
    pJob->realTarget.SetParams( params );

    log->Debug( UtilityMsg, "Target url is: %s",
                pJob->realTarget.GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Open the target file
    //--------------------------------------------------------------------------
    File targetFile;
    OpenFlags::Flags targetFlags = OpenFlags::Update;
    if( pJob->force )
      targetFlags |= OpenFlags::Delete;
    else
      targetFlags |= OpenFlags::New;

    if( pJob->coerce )
      targetFlags |= OpenFlags::Force;

    XRootDStatus st;
    st = targetFile.Open( pJob->realTarget.GetURL(), targetFlags );

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable to open target %s: %s",
                  pJob->realTarget.GetURL().c_str(), st.ToStr().c_str() );
      return st;
    }
    pJob->realTarget = targetFile.GetLastURL();

    //--------------------------------------------------------------------------
    // Generate the source CGI
    //--------------------------------------------------------------------------
    cgiBuff = new char[2048];
    cgiP = XrdOucTPC::cgiC2Src( tpcKey.c_str(),
                                pJob->realTarget.GetHostName().c_str(),
                                -1, cgiBuff, 2048 );
    if( *cgiP == '!' )
    {
      log->Error( UtilityMsg, "Unable to setup source url: %s", cgiP+1 );
      delete [] cgiBuff;
      return XRootDStatus( stError, errInvalidArgs );
    }

    cgiURL.SetParams( cgiBuff );
    delete [] cgiBuff;
    pJob->sources.clear();
    pJob->sources.push_back( pTPCInfo.source );
    params = pJob->sources[0].GetParams();
    MessageUtils::MergeCGI( params, cgiURL.GetParams(), true );
    params["tpc.stage"] = "copy";
    pJob->sources[0].SetParams( params );

    log->Debug( UtilityMsg, "Source url is: %s",
                pJob->sources[0].GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Open the source and set up the rendez-vous
    //--------------------------------------------------------------------------
    File sourceFile;
    st = sourceFile.Open( pJob->sources[0].GetURL(), OpenFlags::Read );

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable to open source %s: %s",
                  pJob->sources[0].GetURL().c_str(), st.ToStr().c_str() );
      targetFile.Close();
      return st;
    }

    st = targetFile.Sync();
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable set up rendez-vous: %s",
                   st.ToStr().c_str() );
      sourceFile.Close();
      targetFile.Close();
      return st;
    }

    //--------------------------------------------------------------------------
    // Do the copy and follow progress
    //--------------------------------------------------------------------------
    TPCStatusHandler  statusHandler;
    Semaphore        *sem  = statusHandler.GetSemaphore();
    StatInfo         *info   = 0;
    FileSystem        fs( pJob->target.GetHostId() );

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
    while( 1 )
    {
      XrdSysTimer::Wait( 1000 );

      if( progress )
      {
        st = fs.Stat( pJob->target.GetPathWithParams(), info );
        if( st.IsOK() )
        {
          progress->JobProgress( info->GetSize(),
                                 pTPCInfo.sourceSize );
          delete info;
          info = 0;
        }
      }

      if( sem->CondWait() )
        break;
    }

    //--------------------------------------------------------------------------
    // Sync has returned so we can check if it was successful
    //--------------------------------------------------------------------------
    st = *statusHandler.GetStatus();

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Third party copy from %s to %s failed: %s",
                  pJob->source.GetURL().c_str(),
                  pJob->target.GetURL().c_str(),
                  st.ToStr().c_str() );
      return st;
    }

    log->Debug( UtilityMsg, "Third party copy from %s to %s successful",
                pJob->source.GetURL().c_str(),
                pJob->target.GetURL().c_str() );

    sourceFile.Close();
    targetFile.Close();

    //--------------------------------------------------------------------------
    // Verify the checksums if needed
    //--------------------------------------------------------------------------
    if( !pJob->checkSumType.empty() )
    {
      log->Debug( UtilityMsg, "Attempting checksum calculation." );

      //------------------------------------------------------------------------
      // Get the check sum at source
      //------------------------------------------------------------------------
      timeval oStart, oEnd;
      XRootDStatus st;
      gettimeofday( &oStart, 0 );
      if( !pJob->checkSumPreset.empty() )
      {
        pJob->sourceCheckSum  = pJob->checkSumType + ":";
        pJob->sourceCheckSum += pJob->checkSumPreset;
      }
      else
      {
        st = Utils::GetRemoteCheckSum( pJob->sourceCheckSum,
                                       pJob->checkSumType,
                                       pJob->source.GetHostId(),
                                       pJob->source.GetPath() );
      }
      gettimeofday( &oEnd, 0 );

      //------------------------------------------------------------------------
      // Print the checksum if so requested and exit
      //------------------------------------------------------------------------
      if( pJob->checkSumPrint )
      {
        std::cerr << std::endl << "CheckSum: ";
        if( !pJob->sourceCheckSum.empty() )
          std::cerr << pJob->sourceCheckSum << std::endl;
        else
          std::cerr << st.ToStr() << std::endl;
        return XRootDStatus();
      }

      if( !st.IsOK() )
        return st;

      //------------------------------------------------------------------------
      // Get the check sum at destination
      //------------------------------------------------------------------------
      timeval tStart, tEnd;
      gettimeofday( &tStart, 0 );
      st = Utils::GetRemoteCheckSum( pJob->targetCheckSum,
                                     pJob->checkSumType,
                                     pJob->target.GetHostId(),
                                     pJob->target.GetPath() );

      if( !st.IsOK() )
        return st;
      gettimeofday( &tEnd, 0 );

      //------------------------------------------------------------------------
      // Compare and inform monitoring
      //------------------------------------------------------------------------
      bool match = false;
      if( pJob->sourceCheckSum == pJob->targetCheckSum )
        match = true;

      Monitor *mon = DefaultEnv::GetMonitor();
      if( mon )
      {
        Monitor::CheckSumInfo i;
        i.transfer.origin = &pJob->source;
        i.transfer.target = &pJob->target;
        i.cksum           = pJob->sourceCheckSum;
        i.oTime           = Utils::GetElapsedMicroSecs( oStart, oEnd );
        i.tTime           = Utils::GetElapsedMicroSecs( tStart, tEnd );
        i.isOK            = match;
        mon->Event( Monitor::EvCheckSum, &i );
      }

      if( !match )
        return XRootDStatus( stError, errCheckSumError, 0 );
    }

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Check whether doing a third party copy is feasible for given
  // job descriptor
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopyJob::CanDo( JobDescriptor *jd, Info *tpcInfo )
  {
    //--------------------------------------------------------------------------
    // Check the initial settings
    //--------------------------------------------------------------------------
    if( !jd->thirdParty )
      return XRootDStatus( stError );

    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Check if third party copy between %s and %s "
                "is possible", jd->source.GetURL().c_str(),
                jd->target.GetURL().c_str() );


    if( jd->source.GetProtocol() != "root" &&
        jd->source.GetProtocol() != "xroot" )
      return XRootDStatus( stError, errNotSupported );

    if( jd->target.GetProtocol() != "root" &&
        jd->target.GetProtocol() != "xroot" )
      return XRootDStatus( stError, errNotSupported );

    //--------------------------------------------------------------------------
    // Check if we can open the source file and whether the actual data server
    // can support the third party copy
    //--------------------------------------------------------------------------
    File          sourceFile;
    XRootDStatus  st;
    URL           sourceURL = jd->source;

    URL::ParamsMap params = sourceURL.GetParams();
    params["tpc.stage"] = "placement";
    sourceURL.SetParams( params );
    log->Debug( UtilityMsg, "Trying to open %s for reading",
                sourceURL.GetURL().c_str() );
    st = sourceFile.Open( sourceURL.GetURL(), OpenFlags::Read );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Cannot open source file %s: %s",
                  jd->source.GetURL().c_str(), st.ToStr().c_str() );
      st.status = stFatal;
      return st;
    }
    tpcInfo->source = sourceFile.GetLastURL();
    StatInfo *statInfo;
    sourceFile.Stat( false, statInfo );
    tpcInfo->sourceSize = statInfo->GetSize();
    delete statInfo;
    sourceFile.Close();

    st = Utils::CheckTPC( tpcInfo->source.GetHostId() );
    if( !st.IsOK() )
      return st;

    //--------------------------------------------------------------------------
    // Verify the destination
    //--------------------------------------------------------------------------
//    st = Utils::CheckTPC( jd->target.GetHostId() );
//    if( !st.IsOK() )
//      return st;
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

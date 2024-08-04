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
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClDlgEnv.hh"
#include "XrdOuc/XrdOucTPC.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

#include <iostream>
#include <chrono>

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
        pSem( new XrdSysSemaphore(0) ), pStatus(0)
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
      XrdSysSemaphore *GetXrdSysSemaphore()
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

      XrdSysSemaphore     *pSem;
      XrdCl::XRootDStatus *pStatus;
  };

  class InitTimeoutCalc
  {
    public:

      InitTimeoutCalc( uint16_t timeLeft ) :
        hasInitTimeout( timeLeft ), start( time( 0 ) ), timeLeft( timeLeft )
      {

      }

      XrdCl::XRootDStatus operator()()
      {
        if( !hasInitTimeout ) return XrdCl::XRootDStatus();

        time_t now = time( 0 );
        if( now - start > timeLeft )
          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errOperationExpired );

        timeLeft -= ( now - start );
        return XrdCl::XRootDStatus();
      }

      operator uint16_t()
      {
        return timeLeft;
      }

    private:
      bool hasInitTimeout;
      time_t start;
      uint16_t timeLeft;
  };

  static XrdCl::XRootDStatus& UpdateErrMsg( XrdCl::XRootDStatus &status, const std::string &str )
  {
    std::string msg = status.GetErrorMessage();
    msg += " (" + str + ")";
    status.SetErrorMessage( msg );
    return status;
  }
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  ThirdPartyCopyJob::ThirdPartyCopyJob( uint16_t      jobId,
                                        PropertyList *jobProperties,
                                        PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults ),
    dstFile( File::DisableVirtRedirect ),
    sourceSize( 0 ),
    initTimeout( 0 ),
    force( false ),
    coerce( false ),
    delegate( false ),
    nbStrm( 0 ),
    tpcLite( false )
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
    Log *log = DefaultEnv::GetLog();

    XRootDStatus st = CanDo();
    if( !st.IsOK() ) return st;

    if( tpcLite )
    {
      //------------------------------------------------------------------------
      // Run TPC-lite algorithm
      //------------------------------------------------------------------------
      XRootDStatus st = RunLite( progress );
      if( !st.IsOK() ) return st;
    }
    else
    {
      //------------------------------------------------------------------------
      // Run vanilla TPC algorithm
      //------------------------------------------------------------------------
      XRootDStatus st = RunTPC( progress );
      if( !st.IsOK() ) return st;
    }

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
      if( checkSumMode == "end2end" || checkSumMode == "source" ||
          !checkSumPreset.empty() )
      {
        gettimeofday( &oStart, 0 );
        if( !checkSumPreset.empty() )
        {
          sourceCheckSum  = checkSumType + ":";
          sourceCheckSum += Utils::NormalizeChecksum( checkSumType,
                                                      checkSumPreset );
        }
        else
        {
          VirtualRedirector *redirector = 0;
          std::string vrCheckSum;
          if( GetSource().IsMetalink() &&
              ( redirector = RedirectorRegistry::Instance().Get( GetSource() ) ) &&
              !( vrCheckSum = redirector->GetCheckSum( checkSumType ) ).empty() )
            sourceCheckSum = vrCheckSum;
          else
            st = Utils::GetRemoteCheckSum( sourceCheckSum, checkSumType, tpcSource );
        }
        gettimeofday( &oEnd, 0 );
        if( !st.IsOK() )
          return UpdateErrMsg( st, "source" );

        pResults->Set( "sourceCheckSum", sourceCheckSum );
      }

      //------------------------------------------------------------------------
      // Get the check sum at destination
      //------------------------------------------------------------------------
      timeval tStart, tEnd;

      if( checkSumMode == "end2end" || checkSumMode == "target" )
      {
        gettimeofday( &tStart, 0 );
        st = Utils::GetRemoteCheckSum( targetCheckSum, checkSumType, realTarget );

        gettimeofday( &tEnd, 0 );
        if( !st.IsOK() )
          return UpdateErrMsg( st, "destination" );
        pResults->Set( "targetCheckSum", targetCheckSum );
      }

      //------------------------------------------------------------------------
      // Make sure the checksums are both lower case
      //------------------------------------------------------------------------
      auto sanitize_cksum = []( char c )
                            {
                              std::locale loc;
                              if( std::isalpha( c ) ) return std::tolower( c, loc );
                              return c;
                            };

      std::transform( sourceCheckSum.begin(), sourceCheckSum.end(),
                      sourceCheckSum.begin(), sanitize_cksum );

      std::transform( targetCheckSum.begin(), targetCheckSum.end(),
                      targetCheckSum.begin(), sanitize_cksum );

      //------------------------------------------------------------------------
      // Compare and inform monitoring
      //------------------------------------------------------------------------
      if( !sourceCheckSum.empty() && !targetCheckSum.empty() )
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

        log->Info(UtilityMsg, "Checksum verification: succeeded." );
      }
    }

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Check whether doing a third party copy is feasible for given
  // job descriptor
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopyJob::CanDo()
  {
    const URL &source = GetSource();
    const URL &target = GetTarget();

    //--------------------------------------------------------------------------
    // We can only do a TPC if both source and destination are remote files
    //--------------------------------------------------------------------------
    if( source.IsLocalFile() || target.IsLocalFile() )
      return XRootDStatus( stError, errNotSupported, 0,
                           "Cannot do a third-party-copy for local file." );

    //--------------------------------------------------------------------------
    // Check the initial settings
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Check if third party copy between %s and %s "
                "is possible", source.GetURL().c_str(),
                target.GetURL().c_str() );

    if( target.GetProtocol() != "root"  &&
        target.GetProtocol() != "xroot" &&
        target.GetProtocol() != "roots" &&
        target.GetProtocol() != "xroots" )
      return XRootDStatus( stError, errNotSupported, 0, "Third-party-copy "
                           "is only supported for root/xroot protocol." );

    pProperties->Get( "initTimeout", initTimeout );
    InitTimeoutCalc timeLeft( initTimeout );

    pProperties->Get( "checkSumMode",    checkSumMode );
    pProperties->Get( "checkSumType",    checkSumType );
    pProperties->Get( "checkSumPreset",  checkSumPreset );
    pProperties->Get( "force",           force );
    pProperties->Get( "coerce",          coerce );
    pProperties->Get( "delegate",        delegate );

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    env->GetInt( "SubStreamsPerChannel", nbStrm );

    // account for the control stream
    if (nbStrm > 0) --nbStrm;

    bool tpcLiteOnly = false;

    if( !delegate )
      log->Info( UtilityMsg, "We are NOT using delegation" );

    //--------------------------------------------------------------------------
    // Resolve the 'auto' checksum type.
    //--------------------------------------------------------------------------
    if( checkSumType == "auto" )
    {
      checkSumType = Utils::InferChecksumType( GetSource(), GetTarget() );
      if( checkSumType.empty() )
        log->Info( UtilityMsg, "Could not infer checksum type." );
      else
        log->Info( UtilityMsg, "Using inferred checksum type: %s.", checkSumType.c_str() );
    }

    //--------------------------------------------------------------------------
    // Check if we can open the source. Note in TPC-lite scenario it is optional
    // for this step to be successful.
    //--------------------------------------------------------------------------
    File           sourceFile;
    XRootDStatus   st;
    URL            sourceURL = source;
    URL::ParamsMap params;

    // set WriteRecovery property
    std::string value;
    DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );
    sourceFile.SetProperty( "ReadRecovery", value );

    // save the original opaque parameter list as specified by the user for later
    const URL::ParamsMap &srcparams = sourceURL.GetParams();

    //--------------------------------------------------------------------------
    // Do the facultative step at source only if the protocol is root/xroot,
    // otherwise don't bother
    //--------------------------------------------------------------------------
    if( sourceURL.GetProtocol() == "root"  || sourceURL.GetProtocol() == "xroot" ||
        sourceURL.GetProtocol() == "roots" || sourceURL.GetProtocol() == "xroots" )
    {
      params = sourceURL.GetParams();
      params["tpc.stage"] = "placement";
      sourceURL.SetParams( params );
      log->Debug( UtilityMsg, "Trying to open %s for reading",
                  sourceURL.GetURL().c_str() );
      st = sourceFile.Open( sourceURL.GetURL(), OpenFlags::Read, Access::None,
                            timeLeft );
    }
    else
      st = XRootDStatus( stError, errNotSupported );

    if( st.IsOK() )
    {
      std::string sourceUrl;
      sourceFile.GetProperty( "LastURL", sourceUrl );
      tpcSource = sourceUrl;

      VirtualRedirector *redirector = 0;
      long long size = -1;
      if( source.IsMetalink() &&
          ( redirector = RedirectorRegistry::Instance().Get( tpcSource ) ) &&
          ( size = redirector->GetSize() ) >= 0 )
        sourceSize = size;
      else
      {
        StatInfo *statInfo;
        st = sourceFile.Stat( false, statInfo );
        if (st.IsOK()) sourceSize = statInfo->GetSize();
        delete statInfo;
      }
    }
    else
    {
      log->Info( UtilityMsg, "Cannot open source file %s: %s",
                  source.GetURL().c_str(), st.ToStr().c_str() );
      if( !delegate )
      {
        //----------------------------------------------------------------------
        // If we cannot contact the source and there is no credential to delegate
        // it cannot possibly work
        //----------------------------------------------------------------------
        st.status = stFatal;
        return st;
      }

      tpcSource   = sourceURL;
      tpcLiteOnly = true;
    }

    // get the opaque parameters as returned by the redirector
    URL tpcSourceUrl = tpcSource;
    URL::ParamsMap tpcsrcparams = tpcSourceUrl.GetParams();
    // merge the original cgi with the one returned by the redirector,
    // the original values take precedence
    URL::ParamsMap::const_iterator itr = srcparams.begin();
    for( ; itr != srcparams.end(); ++itr )
      tpcsrcparams[itr->first] = itr->second;
    tpcSourceUrl.SetParams( tpcsrcparams );
    // save the merged opaque parameter list for later
    std::string scgi;
    const URL::ParamsMap &scgiparams = tpcSourceUrl.GetParams();
    itr = scgiparams.begin();
    for( ; itr != scgiparams.end(); ++itr )
      if( itr->first.compare( 0, 6, "xrdcl." ) != 0 )
      {
        if( !scgi.empty() ) scgi += '\t';
        scgi += itr->first + '=' + itr->second;
      }

    if( !timeLeft().IsOK() )
    {
      // we still want to send a close, but we time it out quickly
      st = sourceFile.Close( 1 );
      return XRootDStatus( stError, errOperationExpired );
    }

    st = sourceFile.Close( timeLeft );

    if( !timeLeft().IsOK() )
      return XRootDStatus( stError, errOperationExpired );

    //--------------------------------------------------------------------------
    // Now we need to check the destination !!!
    //--------------------------------------------------------------------------
    if( delegate )
      DlgEnv::Instance().Enable();
    else
      DlgEnv::Instance().Disable();

    //--------------------------------------------------------------------------
    // Generate the destination CGI
    //--------------------------------------------------------------------------
    log->Debug( UtilityMsg, "Generating the destination TPC URL" );

    tpcKey  = GenerateKey();

    char        *cgiBuff = new char[2048];
    const char  *cgiP    = XrdOucTPC::cgiC2Dst( tpcKey.c_str(),
                                                tpcSource.GetHostId().c_str(),
                                                tpcSource.GetPath().c_str(),
                                                0, cgiBuff, 2048, nbStrm,
                                                GetSource().GetHostId().c_str(),
                                                GetSource().GetProtocol().c_str(),
                                                GetTarget().GetProtocol().c_str(),
                                                delegate );

    if( *cgiP == '!' )
    {
      log->Error( UtilityMsg, "Unable to setup target url: %s", cgiP+1 );
      delete [] cgiBuff;
      return XRootDStatus( stError, errNotSupported );
    }

    URL cgiURL; cgiURL.SetParams( cgiBuff );
    delete [] cgiBuff;

    realTarget = GetTarget();
    params = realTarget.GetParams();
    MessageUtils::MergeCGI( params, cgiURL.GetParams(), true );

    if( !tpcLiteOnly ) // we only append oss.asize if it source file size is actually known
    {
      std::ostringstream o; o << sourceSize;
      params["oss.asize"] = o.str();
    }
    params["tpc.stage"] = "copy";

    // forward source cgi info to the destination in case we are going to do delegation
    if( !scgi.empty() && delegate )
      params["tpc.scgi"] = scgi;

    realTarget.SetParams( params );

    log->Debug( UtilityMsg, "Target url is: %s", realTarget.GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Open the target file
    //--------------------------------------------------------------------------
    // set WriteRecovery property
    DefaultEnv::GetEnv()->GetString( "WriteRecovery", value );
    dstFile.SetProperty( "WriteRecovery", value );

    OpenFlags::Flags targetFlags = OpenFlags::Update;
    if( force )
      targetFlags |= OpenFlags::Delete;
    else
      targetFlags |= OpenFlags::New;

    if( coerce )
      targetFlags |= OpenFlags::Force;

    Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
    st = dstFile.Open( realTarget.GetURL(), targetFlags, mode, timeLeft );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable to open target %s: %s",
                  realTarget.GetURL().c_str(), st.ToStr().c_str() );
      if( st.code == errErrorResponse &&
          st.errNo == kXR_FSError &&
          st.GetErrorMessage().find( "tpc not supported" ) != std::string::npos )
        return XRootDStatus( stError, errNotSupported, 0, // the open failed due to lack of TPC support on the server side
                             "Destination does not support third-party-copy." );
      return UpdateErrMsg( st, "destination" );
    }

    std::string lastUrl; dstFile.GetProperty( "LastURL", lastUrl );
    realTarget = lastUrl;

    if( !timeLeft().IsOK() )
    {
      // we still want to send a close, but we time it out fast
      st = dstFile.Close( 1 );
      return XRootDStatus( stError, errOperationExpired );
    }

    //--------------------------------------------------------------------------
    // Verify if the destination supports TPC / TPC-lite
    //--------------------------------------------------------------------------
    st = Utils::CheckTPCLite( realTarget.GetHostId() );
    if( !st.IsOK() )
    {
      // we still want to send a close, but we time it out fast
      st = dstFile.Close( 1 );
      return XRootDStatus( stError, errNotSupported, 0, // doesn't support TPC
                           "Destination does not support third-party-copy.");
    }

    //--------------------------------------------------------------------------
    // if target supports TPC-lite and we have a credential to delegate we can
    // go ahead and use TPC-lite
    //--------------------------------------------------------------------------
    tpcLite = ( st.code != suPartial ) && delegate;

    if( !tpcLite && tpcLiteOnly ) // doesn't support TPC-lite and it was our only hope
    {
      st = dstFile.Close( 1 );
      return XRootDStatus( stError, errNotSupported, 0, "Destination does not "
                           "support delegation." );
    }

    //--------------------------------------------------------------------------
    // adjust the InitTimeout
    //--------------------------------------------------------------------------
    if( !timeLeft().IsOK() )
    {
      // we still want to send a close, but we time it out fast
      st = dstFile.Close( 1 );
      return XRootDStatus( stError, errOperationExpired );
    }

    //--------------------------------------------------------------------------
    // If we don't use delegation the source has to support TPC
    //--------------------------------------------------------------------------
    if( !tpcLite )
    {
      st = Utils::CheckTPC( URL( tpcSource ).GetHostId(), timeLeft );
      if( !st.IsOK() )
      {
        log->Error( UtilityMsg, "Source (%s) does not support TPC",
                    tpcSource.GetHostId().c_str() );
        return XRootDStatus( stError, errNotSupported, 0, "Source does not "
                             "support third-party-copy" );
      }

      if( !timeLeft().IsOK() )
      {
        // we still want to send a close, but we time it out quickly
        st = sourceFile.Close( 1 );
        return XRootDStatus( stError, errOperationExpired );
      }
    }

    initTimeout = uint16_t( timeLeft );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Run vanilla copy job
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopyJob::RunTPC( CopyProgressHandler *progress )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Generate the source CGI
    //--------------------------------------------------------------------------
    char       *cgiBuff = new char[2048];
    const char *cgiP    = XrdOucTPC::cgiC2Src( tpcKey.c_str(),
                                realTarget.GetHostName().c_str(), -1, cgiBuff,
                                2048 );
    if( *cgiP == '!' )
    {
      log->Error( UtilityMsg, "Unable to setup source url: %s", cgiP+1 );
      delete [] cgiBuff;
      return XRootDStatus( stError, errInvalidArgs );
    }

    URL cgiURL; cgiURL.SetParams( cgiBuff );
    delete [] cgiBuff;
    URL::ParamsMap params = tpcSource.GetParams();
    MessageUtils::MergeCGI( params, cgiURL.GetParams(), true );
    params["tpc.stage"] = "copy";
    tpcSource.SetParams( params );

    log->Debug( UtilityMsg, "Source url is: %s", tpcSource.GetURL().c_str() );

    // Set the close timeout to the default value of the stream timeout
    int closeTimeout = 0;
    (void) DefaultEnv::GetEnv()->GetInt( "StreamTimeout", closeTimeout);

    //--------------------------------------------------------------------------
    // Set up the rendez-vous and open the source
    //--------------------------------------------------------------------------
    InitTimeoutCalc timeLeft( initTimeout );
    XRootDStatus st = dstFile.Sync( timeLeft );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable set up rendez-vous: %s",
                   st.ToStr().c_str() );
      XRootDStatus status = dstFile.Close( closeTimeout );
      return UpdateErrMsg( st, "destination" );
    }

    //--------------------------------------------------------------------------
    // Calculate the time we have left to perform source open
    //--------------------------------------------------------------------------
    if( !timeLeft().IsOK() )
    {
      XRootDStatus status = dstFile.Close( closeTimeout );
      return XRootDStatus( stError, errOperationExpired );
    }

    File sourceFile( File::DisableVirtRedirect );
    // set ReadRecovery property
    std::string value;
    DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );
    sourceFile.SetProperty( "ReadRecovery", value );

    st = sourceFile.Open( tpcSource.GetURL(), OpenFlags::Read, Access::None,
                          timeLeft );

    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable to open source %s: %s",
                  tpcSource.GetURL().c_str(), st.ToStr().c_str() );
      XRootDStatus status = dstFile.Close( closeTimeout );
      return UpdateErrMsg( st, "source" );
    }

    //--------------------------------------------------------------------------
    // Do the copy and follow progress
    //--------------------------------------------------------------------------
    TPCStatusHandler  statusHandler;
    XrdSysSemaphore  *sem  = statusHandler.GetXrdSysSemaphore();
    StatInfo         *info   = 0;

    uint16_t tpcTimeout = 0;
    pProperties->Get( "tpcTimeout", tpcTimeout );

    st = dstFile.Sync( &statusHandler, tpcTimeout );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable start the copy: %s",
                  st.ToStr().c_str() );
      XRootDStatus statusS = sourceFile.Close( closeTimeout );
      XRootDStatus statusT = dstFile.Close( closeTimeout );
      return UpdateErrMsg( st, "destination" );
    }

    //--------------------------------------------------------------------------
    // Stat the file every second until sync returns
    //--------------------------------------------------------------------------
    bool canceled = false;
    while( 1 )
    {
      XrdSysTimer::Wait( 2500 );

      if( progress )
      {
        st = dstFile.Stat( true, info );
        if( st.IsOK() )
        {
          progress->JobProgress( pJobId, info->GetSize(), sourceSize );
          delete info;
          info = 0;
        }
        bool shouldCancel = progress->ShouldCancel( pJobId );
        if( shouldCancel )
        {
          log->Debug( UtilityMsg, "Cancellation requested by progress handler" );
          Buffer arg, *response = 0; arg.FromString( "ofs.tpc cancel" );
          XRootDStatus st = dstFile.Fcntl( arg, response );
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

      // Ignore close response
      XRootDStatus statusS = sourceFile.Close( closeTimeout );
      XRootDStatus statusT = dstFile.Close( closeTimeout );
      return st;
    }

    XRootDStatus statusS = sourceFile.Close( closeTimeout );
    XRootDStatus statusT = dstFile.Close( closeTimeout );

    if ( !statusS.IsOK() || !statusT.IsOK() )
    {
      st = (statusS.IsOK() ? statusT : statusS);
      log->Error( UtilityMsg, "Third party copy from %s to %s failed during "
                  "close of %s: %s", GetSource().GetURL().c_str(),
                  GetTarget().GetURL().c_str(),
                  (statusS.IsOK() ? "destination" : "source"), st.ToStr().c_str() );
      return UpdateErrMsg( st, statusS.IsOK() ? "source" : "destination" );
    }

    log->Debug( UtilityMsg, "Third party copy from %s to %s successful",
                GetSource().GetURL().c_str(), GetTarget().GetURL().c_str() );

    pResults->Set( "size", sourceSize );

    return XRootDStatus();
  }

  XRootDStatus ThirdPartyCopyJob::RunLite( CopyProgressHandler *progress )
  {
    Log *log = DefaultEnv::GetLog();

    // Set the close timeout to the default value of the stream timeout
    int closeTimeout = 0;
    (void) DefaultEnv::GetEnv()->GetInt( "StreamTimeout", closeTimeout);

    //--------------------------------------------------------------------------
    // Set up the rendez-vous
    //--------------------------------------------------------------------------
    InitTimeoutCalc timeLeft( initTimeout );
    XRootDStatus st = dstFile.Sync( timeLeft );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable set up rendez-vous: %s",
                   st.ToStr().c_str() );
      XRootDStatus status = dstFile.Close( closeTimeout );
      return UpdateErrMsg( st, "destination" );
    }

    //--------------------------------------------------------------------------
    // Do the copy and follow progress
    //--------------------------------------------------------------------------
    TPCStatusHandler  statusHandler;
    XrdSysSemaphore  *sem  = statusHandler.GetXrdSysSemaphore();
    StatInfo         *info   = 0;

    uint16_t tpcTimeout = 0;
    pProperties->Get( "tpcTimeout", tpcTimeout );

    st = dstFile.Sync( &statusHandler, tpcTimeout );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Unable start the copy: %s",
                  st.ToStr().c_str() );
      XRootDStatus statusT = dstFile.Close( closeTimeout );
      return UpdateErrMsg( st, "destination" );
    }

    //--------------------------------------------------------------------------
    // Stat the file every second until sync returns
    //--------------------------------------------------------------------------
    bool canceled = false;
    while( 1 )
    {
      XrdSysTimer::Wait( 2500 );

      if( progress )
      {
        st = dstFile.Stat( true, info );
        if( st.IsOK() )
        {
          progress->JobProgress( pJobId, info->GetSize(), sourceSize );
          delete info;
          info = 0;
        }
        bool shouldCancel = progress->ShouldCancel( pJobId );
        if( shouldCancel )
        {
          log->Debug( UtilityMsg, "Cancellation requested by progress handler" );
          Buffer arg, *response = 0; arg.FromString( "ofs.tpc cancel" );
          XRootDStatus st = dstFile.Fcntl( arg, response );
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

      // Ignore close response
      XRootDStatus statusT = dstFile.Close( closeTimeout );
      return st;
    }

    st = dstFile.Close( closeTimeout );

    if ( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Third party copy from %s to %s failed during "
                  "close of %s: %s", GetSource().GetURL().c_str(),
                  GetTarget().GetURL().c_str(),
                  "destination", st.ToStr().c_str() );
      return UpdateErrMsg( st, "destination" );
    }

    log->Debug( UtilityMsg, "Third party copy from %s to %s successful",
                GetSource().GetURL().c_str(), GetTarget().GetURL().c_str() );

    pResults->Set( "size", sourceSize );

    return XRootDStatus();
  }


  //----------------------------------------------------------------------------
  // Generate a rendez-vous key
  //----------------------------------------------------------------------------
  std::string ThirdPartyCopyJob::GenerateKey()
  {
    static const int _10to9 = 1000000000;

    char tpcKey[25];

    auto tp = std::chrono::high_resolution_clock::now();
    auto d  = tp.time_since_epoch();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>( d );
    auto s  = std::chrono::duration_cast<std::chrono::seconds>( d );
    uint32_t k1 = ns.count() - s.count() * _10to9;
    uint32_t k2 = getpid() | (getppid() << 16);
    uint32_t k3 = s.count();
    snprintf( tpcKey, 25, "%08x%08x%08x", k1, k2, k3 );
    return std::string(tpcKey);
  }
}

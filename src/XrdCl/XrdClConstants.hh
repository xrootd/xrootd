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

#ifndef __XRD_CL_CONSTANTS_HH__
#define __XRD_CL_CONSTANTS_HH__

#include <cstdint>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Log message types
  //----------------------------------------------------------------------------
  const uint64_t AppMsg             = 0x0000000000000001ULL;
  const uint64_t UtilityMsg         = 0x0000000000000002ULL;
  const uint64_t FileMsg            = 0x0000000000000004ULL;
  const uint64_t PollerMsg          = 0x0000000000000008ULL;
  const uint64_t PostMasterMsg      = 0x0000000000000010ULL;
  const uint64_t XRootDTransportMsg = 0x0000000000000020ULL;
  const uint64_t TaskMgrMsg         = 0x0000000000000040ULL;
  const uint64_t XRootDMsg          = 0x0000000000000080ULL;
  const uint64_t FileSystemMsg      = 0x0000000000000100ULL;
  const uint64_t AsyncSockMsg       = 0x0000000000000200ULL;
  const uint64_t JobMgrMsg          = 0x0000000000000400ULL;
  const uint64_t PlugInMgrMsg       = 0x0000000000000800ULL;
  const uint64_t ExDbgMsg           = 0x0000000000001000ULL; //special type debugging extra-hard problems
  const uint64_t TlsMsg             = 0x0000000000002000ULL;
  const uint64_t ZipMsg             = 0x0000000000004000ULL;

  //----------------------------------------------------------------------------
  // Environment settings
  //----------------------------------------------------------------------------
  const int DefaultSubStreamsPerChannel    = 1;
  const int DefaultConnectionWindow        = 120;
  const int DefaultConnectionRetry         = 5;
  const int DefaultRequestTimeout          = 1800;
  const int DefaultStreamTimeout           = 60;
  const int DefaultTimeoutResolution       = 15;
  const int DefaultStreamErrorWindow       = 1800;
  const int DefaultRunForkHandler          = 1;
  const int DefaultRedirectLimit           = 16;
  const int DefaultWorkerThreads           = 3;
  const int DefaultCPChunkSize             = 8388608;
  const int DefaultCPParallelChunks        = 4;
  const int DefaultDataServerTTL           = 300;
  const int DefaultLoadBalancerTTL         = 1200;
  const int DefaultCPInitTimeout           = 600;
  const int DefaultCPTPCTimeout            = 1800;
  const int DefaultCPTimeout               = 0;
  const int DefaultTCPKeepAlive            = 0;
  const int DefaultTCPKeepAliveTime        = 7200;
  const int DefaultTCPKeepAliveInterval    = 75;
  const int DefaultTCPKeepAliveProbes      = 9;
  const int DefaultMultiProtocol           = 0;
  const int DefaultParallelEvtLoop         = 1;
  const int DefaultMetalinkProcessing      = 1;
  const int DefaultLocalMetalinkFile       = 0;
  const int DefaultXRateThreshold          = 0;
  const int DefaultXCpBlockSize            = 134217728; // DefaultCPChunkSize * DefaultCPParallelChunks * 2
#ifdef __APPLE__
  // we don't have corking on osx so we cannot turn of nagle
  const int DefaultNoDelay                 = 0;
#else
  const int DefaultNoDelay                 = 1;
#endif
  const int DefaultAioSignal               = 0;
  const int DefaultPreferIPv4              = 0;
  const int DefaultMaxMetalinkWait         = 60;
  const int DefaultPreserveLocateTried     = 1;
  const int DefaultNotAuthorizedRetryLimit = 3;
  const int DefaultPreserveXAttrs          = 0;
  const int DefaultNoTlsOK                 = 0;
  const int DefaultTlsNoData               = 0;
  const int DefaultTlsMetalink             = 0;
  const int DefaultZipMtlnCksum            = 0;
  const int DefaultIPNoShuffle             = 0;
  const int DefaultWantTlsOnNoPgrw         = 0;
  const int DefaultRetryWrtAtLBLimit       = 3;
  const int DefaultCpRetry                 = 0;
  const int DefaultCpUsePgWrtRd            = 1;

  const char * const DefaultPollerPreference   = "built-in";
  const char * const DefaultNetworkStack       = "IPAuto";
  const char * const DefaultClientMonitor      = "";
  const char * const DefaultClientMonitorParam = "";
  const char * const DefaultPlugInConfDir      = "";
  const char * const DefaultPlugIn             = "";
  const char * const DefaultReadRecovery       = "true";
  const char * const DefaultWriteRecovery      = "true";
  const char * const DefaultOpenRecovery       = "true";
  const char * const DefaultGlfnRedirector     = "";
  const char * const DefaultTlsDbgLvl          = "OFF";
  const char * const DefaultClConfDir          = "";
  const char * const DefaultClConfFile         = "";
  const char * const DefaultCpTarget           = "";
  const char * const DefaultCpRetryPolicy      = "force";

  inline static std::string to_lower( std::string str )
  {
    std::transform( str.begin(), str.end(), str.begin(), ::tolower );
    return str;
  }

  static std::unordered_map<std::string, int> theDefaultInts
    {
      { to_lower( "SubStreamsPerChannel" ),    DefaultSubStreamsPerChannel },
      { to_lower( "ConnectionWindow" ),        DefaultConnectionWindow },
      { to_lower( "ConnectionRetry" ),         DefaultConnectionRetry },
      { to_lower( "RequestTimeout" ),          DefaultRequestTimeout },
      { to_lower( "StreamTimeout" ),           DefaultStreamTimeout },
      { to_lower( "TimeoutResolution" ),       DefaultTimeoutResolution },
      { to_lower( "StreamErrorWindow" ),       DefaultStreamErrorWindow },
      { to_lower( "RunForkHandler" ),          DefaultRunForkHandler },
      { to_lower( "RedirectLimit" ),           DefaultRedirectLimit },
      { to_lower( "WorkerThreads" ),           DefaultWorkerThreads },
      { to_lower( "CPChunkSize" ),             DefaultCPChunkSize },
      { to_lower( "CPParallelChunks" ),        DefaultCPParallelChunks },
      { to_lower( "DataServerTTL" ),           DefaultDataServerTTL },
      { to_lower( "LoadBalancerTTL" ),         DefaultLoadBalancerTTL },
      { to_lower( "CPInitTimeout" ),           DefaultCPInitTimeout },
      { to_lower( "CPTPCTimeout" ),            DefaultCPTPCTimeout },
      { to_lower( "CPTimeout" ),               DefaultCPTimeout },
      { to_lower( "TCPKeepAlive" ),            DefaultTCPKeepAlive },
      { to_lower( "TCPKeepAliveTime" ),        DefaultTCPKeepAliveTime },
      { to_lower( "TCPKeepAliveInterval" ),    DefaultTCPKeepAliveInterval },
      { to_lower( "TCPKeepAliveProbes" ),      DefaultTCPKeepAliveProbes },
      { to_lower( "MultiProtocol" ),           DefaultMultiProtocol },
      { to_lower( "ParallelEvtLoop" ),         DefaultParallelEvtLoop },
      { to_lower( "MetalinkProcessing" ),      DefaultMetalinkProcessing },
      { to_lower( "LocalMetalinkFile" ),       DefaultLocalMetalinkFile },
      { to_lower( "XRateThreshold" ),          DefaultXRateThreshold },
      { to_lower( "XCpBlockSize" ),            DefaultXCpBlockSize },
      { to_lower( "NoDelay" ),                 DefaultNoDelay },
      { to_lower( "AioSignal" ),               DefaultAioSignal },
      { to_lower( "PreferIPv4" ),              DefaultPreferIPv4 },
      { to_lower( "MaxMetalinkWait" ),         DefaultMaxMetalinkWait },
      { to_lower( "PreserveLocateTried" ),     DefaultPreserveLocateTried },
      { to_lower( "NotAuthorizedRetryLimit" ), DefaultNotAuthorizedRetryLimit },
      { to_lower( "PreserveXAttrs" ),          DefaultPreserveXAttrs },
      { to_lower( "NoTlsOK" ),                 DefaultNoTlsOK },
      { to_lower( "TlsNoData" ),               DefaultTlsNoData },
      { to_lower( "TlsMetalink" ),             DefaultTlsMetalink },
      { to_lower( "ZipMtlnCksum" ),            DefaultZipMtlnCksum },
      { to_lower( "IPNoShuffle" ),             DefaultIPNoShuffle },
      { to_lower( "WantTlsOnNoPgrw" ),         DefaultWantTlsOnNoPgrw },
      { to_lower( "RetryWrtAtLBLimit" ),       DefaultRetryWrtAtLBLimit }
    };

  static std::unordered_map<std::string, std::string> theDefaultStrs
    {
      { to_lower( "PollerPreference" ),   DefaultPollerPreference },
      { to_lower( "NetworkStack" ),       DefaultNetworkStack },
      { to_lower( "ClientMonitor" ),      DefaultClientMonitor },
      { to_lower( "ClientMonitorParam" ), DefaultClientMonitorParam },
      { to_lower( "PlugInConfDir" ),      DefaultPlugInConfDir },
      { to_lower( "PlugIn" ),             DefaultPlugIn },
      { to_lower( "ReadRecovery" ),       DefaultReadRecovery },
      { to_lower( "WriteRecovery" ),      DefaultWriteRecovery },
      { to_lower( "OpenRecovery" ),       DefaultOpenRecovery },
      { to_lower( "GlfnRedirector" ),     DefaultGlfnRedirector },
      { to_lower( "TlsDbgLvl" ),          DefaultTlsDbgLvl },
      { to_lower( "ClConfDir" ),          DefaultClConfDir },
      { to_lower( "DefaultClConfFile" ),  DefaultClConfFile },
      { to_lower( "CpTarget" ),           DefaultCpTarget }
    };
}

#endif // __XRD_CL_CONSTANTS_HH__

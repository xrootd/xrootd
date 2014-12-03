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

#include <stdint.h>

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

  //----------------------------------------------------------------------------
  // Environment settings
  //----------------------------------------------------------------------------
  const int DefaultSubStreamsPerChannel = 1;
  const int DefaultConnectionWindow     = 120;
  const int DefaultConnectionRetry      = 5;
  const int DefaultRequestTimeout       = 1800;
  const int DefaultStreamTimeout        = 60;
  const int DefaultTimeoutResolution    = 15;
  const int DefaultStreamErrorWindow    = 1800;
  const int DefaultRunForkHandler       = 0;
  const int DefaultRedirectLimit        = 16;
  const int DefaultWorkerThreads        = 3;
  const int DefaultCPChunkSize          = 16777216;
  const int DefaultCPParallelChunks     = 4;
  const int DefaultDataServerTTL        = 300;
  const int DefaultLoadBalancerTTL      = 1200;
  const int DefaultCPInitTimeout        = 600;
  const int DefaultCPTPCTimeout         = 1800;
  const int DefaultTCPKeepAlive         = 0;
  const int DefaultTCPKeepAliveTime     = 7200;
  const int DefaultTCPKeepAliveInterval = 75;
  const int DefaultTCPKeepAliveProbes   = 9;
  const int DefaultMultiProtocol        = 0;

  const char * const DefaultPollerPreference   = "built-in,libevent";
  const char * const DefaultNetworkStack       = "IPAuto";
  const char * const DefaultClientMonitor      = "";
  const char * const DefaultClientMonitorParam = "";
  const char * const DefaultPlugInConfDir      = "";
  const char * const DefaultPlugIn             = "";
}

#endif // __XRD_CL_CONSTANTS_HH__

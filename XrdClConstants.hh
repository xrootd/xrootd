//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_CONSTANTS_HH__
#define __XRD_CL_CONSTANTS_HH__

#include <stdint.h>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Log message types
  //----------------------------------------------------------------------------
  const uint64_t AppMsg             = 0x0000000000000001;
  const uint64_t UtilityMsg         = 0x0000000000000002;
  const uint64_t FileMsg            = 0x0000000000000004;
  const uint64_t PollerMsg          = 0x0000000000000008;
  const uint64_t PostMasterMsg      = 0x0000000000000010;
  const uint64_t XRootDTransportMsg = 0x0000000000000020;
  const uint64_t TaskMgrMsg         = 0x0000000000000040;

  //----------------------------------------------------------------------------
  // Environment settings
  //----------------------------------------------------------------------------
  const int DefaultStreamsPerChannel = 1;
  const int DefaultConnectionWindow  = 120;
  const int DefaultConnectionRetry   = 5;
  const int DefaultRequestTimeout    = 300;
  const int DefaultDataServerTTL     = 300;
  const int DefaultManagerTTL        = 1200;
  const int DefaultTimeoutResolution = 15;
  const int DefaultStreamErrorWindow = 1800;
}

#endif // __XRD_CL_CONSTANTS_HH__

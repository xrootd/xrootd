//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_CONSTANTS_HH__
#define __XRD_CL_CONSTANTS_HH__

#include <stdint.h>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Log message types
  //----------------------------------------------------------------------------
  const uint64_t AppMsg     = 0x0000000000000001;
  const uint64_t UtilityMsg = 0x0000000000000002;

  //----------------------------------------------------------------------------
  // Environment settings
  //----------------------------------------------------------------------------
  const int DefaultConnectionTimeout = 120;
  const int DefaultDataServerTimeout = 300;
  const int DefaultManagerTimeout    = 1200;
}

#endif // __XRD_CL_CONSTANTS_HH__

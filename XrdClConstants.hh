//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_CONSTANTS_HH__
#define __XRD_CL_CONSTANTS_HH__

#include <stdint.h>

namespace XrdCl
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
  const uint64_t XRootDMsg          = 0x0000000000000080;
  const uint64_t QueryMsg           = 0x0000000000000100;

  //----------------------------------------------------------------------------
  // Environment settings
  //----------------------------------------------------------------------------
  const int DefaultSubStreamsPerChannel = 1;
  const int DefaultConnectionWindow     = 120;
  const int DefaultConnectionRetry      = 5;
  const int DefaultRequestTimeout       = 300;
  const int DefaultDataServerTTL        = 300;
  const int DefaultManagerTTL           = 1200;
  const int DefaultTimeoutResolution    = 15;
  const int DefaultStreamErrorWindow    = 1800;
}

#endif // __XRD_CL_CONSTANTS_HH__

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

#ifndef __XRD_CL_STOP_AT_EXIT_HH__
#define __XRD_CL_STOP_AT_EXIT_HH__

// This header is not installed.

#include <atomic>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! True when the exit handler of the client stops its threads. After this
  //! stop, no response comes, thus the post master refuses new requests,
  //! for example of an exit handler of the program which runs later.
  //----------------------------------------------------------------------------
  extern std::atomic<bool> gStoppedAtExit;
}

#endif // __XRD_CL_STOP_AT_EXIT_HH__

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

#ifndef __XRD_CL_SID_MANAGER_HH__
#define __XRD_CL_SID_MANAGER_HH__

#include <list>
#include <stdint.h>
#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClStatus.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Handle XRootD stream IDs
  //----------------------------------------------------------------------------
  class SIDManager
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      SIDManager(): pSIDCeiling(0) {}

      //------------------------------------------------------------------------
      //! Allocate a SID
      //!
      //! @param sid a two byte array where the allocated SID should be stored
      //! @return    stOK on success, stError on error
      //------------------------------------------------------------------------
      Status AllocateSID( uint8_t sid[2] );

      //------------------------------------------------------------------------
      //! Release the SID that is no longer needed
      //------------------------------------------------------------------------
      void ReleaseSID( uint8_t sid[2] );

    private:
      std::list<uint16_t> pFreeSIDs;
      uint16_t            pSIDCeiling;
      XrdSysMutex         pMutex;
  };
}

#endif // __XRD_CL_SID_MANAGER_HH__

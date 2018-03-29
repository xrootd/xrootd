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

#ifndef __XRD_CL_SID_MANAGER_HH__
#define __XRD_CL_SID_MANAGER_HH__

#include <list>
#include <set>
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
      SIDManager(): pSIDCeiling(1) {}

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

      //------------------------------------------------------------------------
      //! Register a SID of a request that timed out
      //------------------------------------------------------------------------
      void TimeOutSID( uint8_t sid[2] );

      //------------------------------------------------------------------------
      //! Check if a SID is timed out
      //------------------------------------------------------------------------
      bool IsTimedOut( uint8_t sid[2] );

      //------------------------------------------------------------------------
      //! Release a timed out SID
      //------------------------------------------------------------------------
      void ReleaseTimedOut( uint8_t sid[2] );

      //------------------------------------------------------------------------
      //! Release all timed out SIDs
      //------------------------------------------------------------------------
      void ReleaseAllTimedOut();

      //------------------------------------------------------------------------
      //! Number of timeout sids
      //------------------------------------------------------------------------
      uint32_t NumberOfTimedOutSIDs() const
      {
        XrdSysMutexHelper scopedLock( pMutex );
        return pTimeOutSIDs.size();
      }

      //------------------------------------------------------------------------
      //! Number of allocated streams
      //------------------------------------------------------------------------
      uint16_t GetNumberOfAllocatedSIDs() const;

    private:
      std::list<uint16_t>  pFreeSIDs;
      std::set<uint16_t>   pTimeOutSIDs;
      uint16_t             pSIDCeiling;
      mutable XrdSysMutex  pMutex;
  };
}

#endif // __XRD_CL_SID_MANAGER_HH__

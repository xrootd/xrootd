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

#include "XrdCl/XrdClSIDManager.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Allocate a SID
  //---------------------------------------------------------------------------
  Status SIDManager::AllocateSID( uint8_t sid[2] )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    uint16_t allocSID = 1;

    //--------------------------------------------------------------------------
    // Get a SID from the list of free SIDs if it's not empty
    //--------------------------------------------------------------------------
    if( !pFreeSIDs.empty() )
    {
      allocSID = pFreeSIDs.front();
      pFreeSIDs.pop_front();
    }
    //--------------------------------------------------------------------------
    // Allocate a new SID if possible
    //--------------------------------------------------------------------------
    else
    {
      if( pSIDCeiling == 0xffff )
        return Status( stError, errNoMoreFreeSIDs );
      allocSID = pSIDCeiling++;
    }

    memcpy( sid, &allocSID, 2 );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Release the SID that is no longer needed
  //----------------------------------------------------------------------------
  void SIDManager::ReleaseSID( uint8_t sid[2] )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    uint16_t relSID = 0;
    memcpy( &relSID, sid, 2 );
    pFreeSIDs.push_back( relSID );
  }

  //----------------------------------------------------------------------------
  // Register a SID of a request that timed out
  //----------------------------------------------------------------------------
  void SIDManager::TimeOutSID( uint8_t sid[2] )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    uint16_t tiSID = 0;
    memcpy( &tiSID, sid, 2 );
    pTimeOutSIDs.insert( tiSID );
  }

  //----------------------------------------------------------------------------
  // Check if a SID is timed out
  //----------------------------------------------------------------------------
  bool SIDManager::IsTimedOut( uint8_t sid[2] )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    uint16_t tiSID = 0;
    memcpy( &tiSID, sid, 2 );
    std::set<uint16_t>::iterator it = pTimeOutSIDs.find( tiSID );
    if( it != pTimeOutSIDs.end() )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Release a timed out SID
  //-----------------------------------------------------------------------------
  void SIDManager::ReleaseTimedOut( uint8_t sid[2] )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    uint16_t tiSID = 0;
    memcpy( &tiSID, sid, 2 );
    pTimeOutSIDs.erase( tiSID );
    pFreeSIDs.push_back( tiSID );
  }

  //------------------------------------------------------------------------
  // Release all timed out SIDs
  //------------------------------------------------------------------------
  void SIDManager::ReleaseAllTimedOut()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    std::set<uint16_t>::iterator it;
    for( it = pTimeOutSIDs.begin(); it != pTimeOutSIDs.end(); ++it )
      pFreeSIDs.push_back( *it );
    pTimeOutSIDs.clear();
  }

  //----------------------------------------------------------------------------
  // Get number of allocated SIDs
  //----------------------------------------------------------------------------
  uint16_t SIDManager::GetNumberOfAllocatedSIDs() const
  {
    XrdSysMutexHelper scopedLock( pMutex );
    return pSIDCeiling - pFreeSIDs.size() - pTimeOutSIDs.size() - 1;
  }
}

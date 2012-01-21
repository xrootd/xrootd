//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClSIDManager.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Allocate a SID
  //---------------------------------------------------------------------------
  Status SIDManager::AllocateSID( uint8_t sid[2] )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    uint16_t allocSID = 0;

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
}

//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
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

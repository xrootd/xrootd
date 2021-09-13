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
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>
#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // We need the forward declaration for the friendship to work properly
  //----------------------------------------------------------------------------
  class SIDMgrPool;

  //----------------------------------------------------------------------------
  //! Handle XRootD stream IDs
  //----------------------------------------------------------------------------
  class SIDManager
  {
    friend class SIDMgrPool;

    private:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      SIDManager(): pSIDCeiling(1), pRefCount(0) { }

#if __cplusplus < 201103L
    //------------------------------------------------------------------------
    // For older complilers we have to make the destructor public, although
    // the shared_pointer is using a custom deleter. It will go away once
    // we drop SLC6 support.
    //------------------------------------------------------------------------
    public:
#endif
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~SIDManager() { }

    public:

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
      mutable size_t       pRefCount;
  };

  //----------------------------------------------------------------------------
  //! Pool of SID manager objects
  //----------------------------------------------------------------------------
  class SIDMgrPool
  {
    public:

      //------------------------------------------------------------------------
      //! @return : instance of SIDMgrPool
      //------------------------------------------------------------------------
      static SIDMgrPool& Instance()
      {
        //----------------------------------------------------------------------
        // We could also use a nifty counter but this is simpler and will do!
        //----------------------------------------------------------------------
        static SIDMgrPool *instance = new SIDMgrPool();
        return *instance;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~SIDMgrPool() { }

      //------------------------------------------------------------------------
      //! @param url : URL for which we need a SIDManager
      //! @return    : a shared pointer to SIDManager object, the pointer has
      //               a custom deleter that will return the object to the pool
      //------------------------------------------------------------------------
      std::shared_ptr<SIDManager> GetSIDMgr( const URL &url );

      //------------------------------------------------------------------------
      //! @param mgr : the SIDManager object to be recycled
      //------------------------------------------------------------------------
      void Recycle( SIDManager *mgr );

    private:

      //------------------------------------------------------------------------
      //! A functional object for handling the deletion of SIDManager objects
      //------------------------------------------------------------------------
      struct RecycleSidMgr
      {
        inline void operator()( SIDManager *mgr )
        {
          SIDMgrPool &pool = SIDMgrPool::Instance();
          pool.Recycle( mgr );
        }
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      SIDMgrPool() { }

      //------------------------------------------------------------------------
      //! Deleted constructors
      //------------------------------------------------------------------------
      SIDMgrPool( const SIDMgrPool& ) = delete;
      SIDMgrPool( SIDMgrPool&& ) = delete;

      //------------------------------------------------------------------------
      //! Deleted assigment operators
      //------------------------------------------------------------------------
      SIDMgrPool& operator=( const SIDMgrPool& ) = delete;
      SIDMgrPool& operator=( SIDMgrPool&& ) = delete;

      XrdSysMutex                                  mtx;
      std::unordered_map<std::string, SIDManager*> pool;
  };
}

#endif // __XRD_CL_SID_MANAGER_HH__

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

#ifndef __XRD_CL_REQUEST_SYNC_HH__
#define __XRD_CL_REQUEST_SYNC_HH__

#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! A helper running a fixed number of requests at a given time
  //----------------------------------------------------------------------------
  class RequestSync
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param reqTotal total number of requests
      //! @param reqQuota number of requests to be run in parallel
      //------------------------------------------------------------------------
      RequestSync( uint32_t reqTotal, uint32_t reqQuota ):
        pQuotaSem( new XrdSysSemaphore( reqQuota ) ),
        pTotalSem( new XrdSysSemaphore( 0 ) ),
        pRequestsLeft( reqTotal ),
        pFailureCounter( 0 )
      {
        if( !reqTotal )
          pTotalSem->Post();
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~RequestSync()
      {
        delete pQuotaSem;
        delete pTotalSem;
      }

      //------------------------------------------------------------------------
      //! Wait for the request quota
      //------------------------------------------------------------------------
      void WaitForQuota()
      {
        pQuotaSem->Wait();
      }

      //------------------------------------------------------------------------
      //! Wait for all the requests to be finished
      //------------------------------------------------------------------------
      void WaitForAll()
      {
        pTotalSem->Wait();
      }

      //------------------------------------------------------------------------
      //! Report the request finish
      //------------------------------------------------------------------------
      void TaskDone( bool success = true )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        if( !success )
          ++pFailureCounter;
        --pRequestsLeft;
        pQuotaSem->Post();
        if( !pRequestsLeft )
          pTotalSem->Post();
      }

      //------------------------------------------------------------------------
      //! Number of tasks finishing with an error
      //------------------------------------------------------------------------
      uint32_t FailureCount() const
      {
        return pFailureCounter;
      }

    private:
      XrdSysMutex      pMutex;
      XrdSysSemaphore *pQuotaSem;
      XrdSysSemaphore *pTotalSem;
      uint32_t         pRequestsLeft;
      uint32_t         pFailureCounter;
  };
}

#endif // __XRD_CL_REQUEST_SYNC_HH__

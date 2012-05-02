//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
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
        pQuotaSem( reqQuota ),
        pTotalSem( 0 ),
        pRequestsLeft( reqTotal ),
        pFailureCounter( 0 )
      {
        if( !reqTotal )
          pTotalSem.Post();
      }

      //------------------------------------------------------------------------
      //! Wait for the request quota
      //------------------------------------------------------------------------
      void WaitForQuota()
      {
        pQuotaSem.Wait();
      }

      //------------------------------------------------------------------------
      //! Wait for all the requests to be finished
      //------------------------------------------------------------------------
      void WaitForAll()
      {
        pTotalSem.Wait();
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
        pQuotaSem.Post();
        if( !pRequestsLeft )
          pTotalSem.Post();
      }

      //------------------------------------------------------------------------
      //! Number of tasks finishig with an error
      //------------------------------------------------------------------------
      uint32_t FailureCount() const
      {
        return pFailureCounter;
      }

    private:
      XrdSysMutex     pMutex;
      XrdSysSemaphore pQuotaSem;
      XrdSysSemaphore pTotalSem;
      uint32_t        pRequestsLeft;
      uint32_t        pFailureCounter;
  };
}

#endif // __XRD_CL_REQUEST_SYNC_HH__

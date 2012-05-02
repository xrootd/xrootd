//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_COPY_PROCESS_HH__
#define __XRD_CL_COPY_PROCESS_HH__

#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include <stdint.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Interface for copy progress notification
  //----------------------------------------------------------------------------
  class CopyProgressHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Notify abut the progress of copy operations
      //!
      //! @param jobNum         the job number of the copy job concerned
      //! @param jobTotal       total number of jobs being processed
      //! @param source         the source url of the current job
      //! @param destination    the destination url of the current job
      //! @param bytesProcessed bytes processed by the current job
      //! @param bytesTotal     total number of bytes to be processed by the 
      //!                       current job
      //------------------------------------------------------------------------
      virtual void ProgressNotify( uint16_t   jobNum,
                                   uint16_t   jobTotal,
                                   const URL &source,
                                   const URL &destination,
                                   uint64_t   bytesProcessed,
                                   uint64_t   bytesTotal ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Copy job
  //----------------------------------------------------------------------------
  class CopyJob
  {
    public:
      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param handler the handler to be notified about the copy progress
      //! @return        status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *handler = 0 ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Copy the data from one point to another
  //----------------------------------------------------------------------------
  class CopyProcess
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CopyProcess( const URL &source, const URL &destination ):
        pSource( source ),
        pDestination( destination ),
        pRecursive( false ),
        pThirdParty( false ),
        pSourceLimit( 1 ),
        pProgressHandler( 0 ) {}

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CopyProcess( const std::string &source, const std::string &destination ):
        pSource( source ),
        pDestination( destination ),
        pRecursive( false ),
        pThirdParty( false ),
        pSourceLimit( 1 ),
        pProgressHandler( 0 ) {}

      //------------------------------------------------------------------------
      //! Perform a recursive copy
      //------------------------------------------------------------------------
      void SetRecursive( bool recursive )
      {
        pRecursive = recursive;
      }

      //------------------------------------------------------------------------
      //! Limit number of possibly chunk sources per job
      //------------------------------------------------------------------------
      void SetSourceLimit( uint16_t sourceLimit )
      {
        pSourceLimit = sourceLimit;
      }

      //------------------------------------------------------------------------
      //! Perform third party copy whenever possible
      //------------------------------------------------------------------------
      void SetThirdPartyCopy( bool thirdParty )
      {
        pThirdParty = thirdParty;
      }

      //------------------------------------------------------------------------
      //! Monitor the progress with the given handler
      //------------------------------------------------------------------------
      void SetProgressHandler( CopyProgressHandler *handler )
      {
        pProgressHandler = handler;
      }

      //------------------------------------------------------------------------
      //! Run the copy jobs
      //------------------------------------------------------------------------
      XRootDStatus Run();

    private:
      URL                  pSource;
      URL                  pDestination;
      bool                 pRecursive;
      bool                 pThirdParty;
      uint16_t             pSourceLimit;
      CopyProgressHandler *pProgressHandler;
  };
}

#endif // __XRD_CL_COPY_PROCESS_HH__

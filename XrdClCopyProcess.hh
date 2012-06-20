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
      //! Notify when a new job is about to start
      //!
      //! @param jobNum         the job number of the copy job concerned
      //! @param jobTotal       total number of jobs being processed
      //! @param source         the source url of the current job
      //! @param destination    the destination url of the current job
      //------------------------------------------------------------------------
      virtual void BeginJob( uint16_t   jobNum,
                             uint16_t   jobTotal,
                             const URL *source,
                             const URL *destination ) = 0;

      //------------------------------------------------------------------------
      //! Notify when the previous job has finished
      //!
      //! @param status status of the job
      //------------------------------------------------------------------------
      virtual void EndJob( const XRootDStatus &status ) = 0;

      //------------------------------------------------------------------------
      //! Notify about the progress of the current job
      //!
      //! @param bytesProcessed bytes processed by the current job
      //! @param bytesTotal     total number of bytes to be processed by the 
      //!                       current job
      //------------------------------------------------------------------------
      virtual void JobProgress( uint64_t bytesProcessed,
                                uint64_t bytesTotal ) = 0;
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
      //! @param progress the handler to be notified about the copy progress
      //! @return         status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *progress = 0 ) = 0;

      //------------------------------------------------------------------------
      //! Get source URL
      //------------------------------------------------------------------------
      const URL *GetSource() const
      {
        return pSource;
      }

      //------------------------------------------------------------------------
      //! Get destination URL
      //------------------------------------------------------------------------
      const URL *GetDestination() const
      {
        return pDestination;
      }

    protected:
      const URL *pSource;
      const URL *pDestination;
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
      CopyProcess():
        pDestination( 0 ),
        pRecursive( false ),
        pThirdParty( false ),
        pSourceLimit( 1 ),
        pProgressHandler( 0 ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CopyProcess();

      //------------------------------------------------------------------------
      //! Add source
      //------------------------------------------------------------------------
      bool AddSource( const std::string &source );

      //------------------------------------------------------------------------
      //! Add source
      //------------------------------------------------------------------------
      bool AddSource( const URL &source );

      //------------------------------------------------------------------------
      //! Set destination
      //------------------------------------------------------------------------
      bool SetDestination( const std::string &destination );

      //------------------------------------------------------------------------
      //! Set destination
      //------------------------------------------------------------------------
      bool SetDestination( const URL &destination );

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
      // Prepare the copy jobs
      //------------------------------------------------------------------------
      XRootDStatus Prepare();

      //------------------------------------------------------------------------
      //! Run the copy jobs
      //------------------------------------------------------------------------
      XRootDStatus Run();

    private:
      std::list<URL*>      pSource;
      std::list<CopyJob*>  pJobs;
      URL                 *pDestination;
      bool                 pRecursive;
      bool                 pThirdParty;
      uint16_t             pSourceLimit;
      CopyProgressHandler *pProgressHandler;
  };
}

#endif // __XRD_CL_COPY_PROCESS_HH__

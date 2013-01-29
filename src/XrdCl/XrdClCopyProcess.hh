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
      virtual ~CopyProgressHandler() {}

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
  //! Job description
  //----------------------------------------------------------------------------
  struct JobDescriptor
  {
    JobDescriptor(): sourceLimit(1), force(false), posc(false),
      thirdParty(false), checkSumPrint(false)
    {}

    URL              source;               //!< [in] original source URL
    URL              target;               //!< [in] target directory or file
    uint16_t         sourceLimit;          //!< [in] max number of download
                                           //!< sources
    bool             force;                //!< [in] overwrite target if exists
    bool             posc;                 //!< [in] Persistify on successful
                                           //!< close
    bool             thirdParty;           //!< [in] do third party copy if
                                           //!< possible
    bool             thirdPartyFallBack;   //!< [in] fall back to classic copy
                                           //!< when it is impossible to do
                                           //!< 3rd party
    bool             checkSumPrint;        //!< [in] print checksum after the
                                           //!< transfer
    std::string      checkSumType;         //!< [in] type of the checksum
    std::string      checkSumPreset;       //!< [in] checksum preset

    std::string      sourceCheckSum;       //!< [out] checksum calculated at
                                           //!< source
    std::string      targetCheckSum;       //!< [out] checksum calculated at
                                           //!< target
    XRootDStatus     status;               //!< [out] status of the copy
                                           //!< operation
    std::vector<URL> sources;              //!< [out] all the possible sources
                                           //!< that may have been located
    URL              realTarget;           //!< the actual disk server target
  };

  //----------------------------------------------------------------------------
  //! Copy job
  //----------------------------------------------------------------------------
  class CopyJob
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CopyJob( JobDescriptor *jobDesc ):
        pJob( jobDesc ) {}

      //------------------------------------------------------------------------
      //! Virtual destructor
      //------------------------------------------------------------------------
      virtual ~CopyJob()
      {
      }

      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param progress the handler to be notified about the copy progress
      //! @return         status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *progress = 0 ) = 0;

      //------------------------------------------------------------------------
      //! Get the job descriptor
      //------------------------------------------------------------------------
      JobDescriptor *GetDescriptor() const
      {
        return pJob;
      }

    protected:
      JobDescriptor *pJob;
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
      CopyProcess() {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CopyProcess();

      //------------------------------------------------------------------------
      //! Add job - it's user's responsibility to handle these after the
      //! copy has bee done
      //------------------------------------------------------------------------
      bool AddJob( JobDescriptor *job )
      {
        pJobDescs.push_back( job );
      }

      //------------------------------------------------------------------------
      // Prepare the copy jobs
      //------------------------------------------------------------------------
      XRootDStatus Prepare();

      //------------------------------------------------------------------------
      //! Run the copy jobs
      //------------------------------------------------------------------------
      XRootDStatus Run( CopyProgressHandler *handler );

    private:
      void CleanUpJobs();
      std::list<JobDescriptor*>  pJobDescs;
      std::list<CopyJob*>        pJobs;
  };
}

#endif // __XRD_CL_COPY_PROCESS_HH__

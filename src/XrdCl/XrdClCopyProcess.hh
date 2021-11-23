//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_COPY_PROCESS_HH__
#define __XRD_CL_COPY_PROCESS_HH__

#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClPropertyList.hh"
#include <cstdint>
#include <vector>

namespace XrdCl
{
  class CopyJob;

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
                             const URL *destination )
      {
        (void)jobNum; (void)jobTotal; (void)source; (void)destination;
      };

      //------------------------------------------------------------------------
      //! Notify when the previous job has finished
      //!
      //! @param jobNum job number
      //! @param result result of the job
      //------------------------------------------------------------------------
      virtual void EndJob( uint16_t            jobNum,
                           const PropertyList *result )
      {
        (void)jobNum; (void)result;
      };

      //------------------------------------------------------------------------
      //! Notify about the progress of the current job
      //!
      //! @param jobNum         job number
      //! @param bytesProcessed bytes processed by the current job
      //! @param bytesTotal     total number of bytes to be processed by the 
      //!                       current job
      //------------------------------------------------------------------------
      virtual void JobProgress( uint16_t jobNum,
                                uint64_t bytesProcessed,
                                uint64_t bytesTotal )
      {
        (void)jobNum; (void)bytesProcessed; (void)bytesTotal;
      };

      //------------------------------------------------------------------------
      //! Determine whether the job should be canceled
      //------------------------------------------------------------------------
      virtual bool ShouldCancel( uint16_t jobNum )
      {
        (void)jobNum;
        return false;
      }
  };

  //----------------------------------------------------------------------------
  // Forward declaration of implementation holding CopyProcess' data members
  //----------------------------------------------------------------------------
  struct CopyProcessImpl;

  //----------------------------------------------------------------------------
  //! Copy the data from one point to another
  //----------------------------------------------------------------------------
  class CopyProcess
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CopyProcess();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CopyProcess();

      //------------------------------------------------------------------------
      //! Add job
      //!
      //! @param properties job configuration parameters
      //! @param results    placeholder for the results
      //!
      //! Configuration properties:
      //! source         [string]   - original source URL
      //! target         [string]   - target directory or file
      //! sourceLimit    [uint16_t] - maximum number sources
      //! force          [bool]     - overwrite target if exists
      //! posc           [bool]     - persistify only on successful close
      //! coerce         [bool]     - ignore locking semantics on destination
      //! makeDir        [bool]     - create path to the file if it doesn't
      //!                             exist
      //! thirdParty     [string]   - "first" try third party copy, if it fails
      //!                             try normal copy; "only" only try third
      //!                             party copy
      //! checkSumMode   [string]   - "none"    - no checksumming
      //!                             "end2end" - end to end checksumming
      //!                             "source"  - calculate checksum at source
      //!                             "target"  - calculate checksum at target
      //! checkSumType   [string]   - type of the checksum to be used
      //! checkSumPreset [string]   - checksum preset
      //! chunkSize      [uint32_t] - size of a copy chunks in bytes
      //! parallelChunks [uint8_t]  - number of chunks that should be requested
      //!                             in parallel
      //! initTimeout    [uint16_t] - time limit for successfull initialization
      //!                             of the copy job
      //! tpcTimeout     [uint16_t] - time limit for the actual copy to finish
      //! dynamicSource  [bool]     - support for the case where the size source
      //!                             file may change during reading process
      //!
      //! Configuration job - this is a job that that is supposed to configure
      //! the copy process as a whole instead of adding a copy job:
      //!
      //! jobType        [string]   - "configuration" - for configuraion
      //! parallel       [uint8_t]  - nomber of copy jobs to be run in parallel
      //!
      //! Results:
      //! sourceCheckSum [string]   - checksum at source, if requested
      //! targetCheckSum [string]   - checksum at target, if requested
      //! size           [uint64_t] - file size
      //! status         [XRootDStatus] - status of the copy operation
      //! sources        [vector<string>] - all sources used
      //! realTarget     [string]   - the actual disk server target
      //------------------------------------------------------------------------
      XRootDStatus AddJob( const PropertyList &properties,
                           PropertyList       *results );

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

      //------------------------------------------------------------------------
      //! Mark the URLs in the property list as ment for TPC
      //------------------------------------------------------------------------
      inline static void MarkTPC( PropertyList &properties )
      {
        std::string keys[] = { "source", "target" };
        size_t      size   = sizeof( keys ) / sizeof( std::string );
        for( size_t i = 0; i < size; ++i )
        {
          URL url;
          properties.Get( keys[i], url );
          URL::ParamsMap params = url.GetParams();
          params["xrdcl.intent"] = "tpc";
          url.SetParams( params );
          properties.Set( keys[i], url.GetURL() );
        }
      }

      //------------------------------------------------------------------------
      //! Pointer to implementation
      //------------------------------------------------------------------------
      CopyProcessImpl *pImpl;
  };
}

#endif // __XRD_CL_COPY_PROCESS_HH__

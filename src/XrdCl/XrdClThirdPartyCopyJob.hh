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

#ifndef __XRD_CL_THIRD_PARTY_COPY_JOB_HH__
#define __XRD_CL_THIRD_PARTY_COPY_JOB_HH__

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClCopyJob.hh"
#include "XrdCl/XrdClFile.hh"

namespace XrdCl
{
  class File;

  class ThirdPartyCopyJob: public CopyJob
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ThirdPartyCopyJob( uint32_t      jobId,
                         PropertyList *jobProperties,
                         PropertyList *jobResults );

      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param progress the handler to be notified about the copy progress
      //! @return         status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *progress = 0 );

    private:

      //------------------------------------------------------------------------
      //! Check whether doing a third party copy is feasible for given
      //! job descriptor
      //!
      //! @param  property list - may be extended by info needed for TPC
      //! @return error when a third party copy cannot be performed and
      //!         fatal error when no copy can be performed
      //------------------------------------------------------------------------
      XRootDStatus CanDo();

      //------------------------------------------------------------------------
      //! Run vanilla copy job
      //------------------------------------------------------------------------
      XRootDStatus RunTPC( CopyProgressHandler *progress );

      //------------------------------------------------------------------------
      //! Run TPC-lite copy job
      //------------------------------------------------------------------------
      XRootDStatus RunLite( CopyProgressHandler *progress );

      //------------------------------------------------------------------------
      //! Generate TPC key
      //------------------------------------------------------------------------
      static std::string GenerateKey();

      XrdCl::File dstFile;
      URL         tpcSource;
      URL         realTarget;
      std::string tpcKey;

      std::string checkSumMode;
      std::string checkSumType;
      std::string checkSumPreset;
      uint64_t    sourceSize;
      time_t      initTimeout;
      bool        force;
      bool        coerce;
      bool        delegate;
      int         nbStrm;
      bool        tpcLite;
  };
}

#endif // __XRD_CL_THIRD_PARTY_COPY_JOB_HH__

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

namespace XrdCl
{
  class ThirdPartyCopyJob: public CopyJob
  {
    public:
      struct Info
      {
        Info(): sourceSize(0) {}
        URL         source;            //!< actual source data server
        std::string sourceChecksum;    //!< checksum at source
        uint64_t    sourceSize;        //!< size of the source file
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ThirdPartyCopyJob( JobDescriptor *jobDesc, Info *tpcInfo );

      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param progress the handler to be notified about the copy progress
      //! @return         status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *progress = 0 );

      //------------------------------------------------------------------------
      //! Check whether doing a third party copy is feasible for given
      //! job descriptor
      //!
      //! @param  jd job descriptor
      //! @param  info a placeholder for the extra info needed by the copy
      //! @return error when a third party copy cannot be performed and
      //!         fatal error when no copy can be performed
      //------------------------------------------------------------------------
      static XRootDStatus CanDo( JobDescriptor *jd, Info *tpcInfo );

    private:
      static std::string GenerateKey();
      Info pTPCInfo;
  };
}

#endif // __XRD_CL_THIRD_PARTY_COPY_JOB_HH__

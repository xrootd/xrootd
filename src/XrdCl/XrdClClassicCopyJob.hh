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

#ifndef __XRD_CL_CLASSIC_COPY_JOB_HH__
#define __XRD_CL_CLASSIC_COPY_JOB_HH__

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClCopyJob.hh"

namespace XrdCl
{
  class ClassicCopyJob: public CopyJob
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      ClassicCopyJob( uint32_t      jobId,
                      PropertyList *jobProperties,
                      PropertyList *jobResults );

      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param progress the handler to be notified about the copy progress
      //! @return         status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *progress = 0 );

      //------------------------------------------------------------------------
      // Get the final result
      //------------------------------------------------------------------------
      inline const XRootDStatus& GetResult() const
      {
        return result;
      }

    private:

      //------------------------------------------------------------------------
      // Update the final result so it is clear that it is a source error
      //------------------------------------------------------------------------
      inline XrdCl::XRootDStatus& SourceError( XrdCl::XRootDStatus &status )
      {
        std::string msg = status.GetErrorMessage();
        msg += " (source)";
        status.SetErrorMessage( msg );
        result = status;
        return status;
      }

      //------------------------------------------------------------------------
      // Update the final result do it is clear that it is a destination error
      //------------------------------------------------------------------------
      inline XrdCl::XRootDStatus& DestinationError( XrdCl::XRootDStatus &status )
      {
        std::string msg = status.GetErrorMessage();
        msg += " (destination)";
        status.SetErrorMessage( msg );
        result = status;
        return status;
      }

      //------------------------------------------------------------------------
      // Set the final result
      //------------------------------------------------------------------------
      template<typename ... Args>
      inline XRootDStatus& SetResult( Args&&... args )
      {
        result = XrdCl::XRootDStatus( std::forward<Args>(args)... );
        return result;
      }

      //------------------------------------------------------------------------
      // The final result
      //------------------------------------------------------------------------
      XRootDStatus result;
  };
}

#endif // __XRD_CL_CLASSIC_COPY_JOB_HH__

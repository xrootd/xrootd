//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//-----------------------------------------------------------------------------
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

#ifndef __XRD_CL_COPY_JOB_HH__
#define __XRD_CL_COPY_JOB_HH__

#include "XrdCl/XrdClPropertyList.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Copy job
  //----------------------------------------------------------------------------
  class CopyJob
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CopyJob( PropertyList *jobProperties, PropertyList *jobResults ):
        pProperties( jobProperties ),
        pResults( jobResults )
      {
        pProperties->Get( "source", pSource );
        pProperties->Get( "target", pTarget );
      }

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
      //! Get the job properties
      //------------------------------------------------------------------------
      PropertyList *GetProperties()
      {
        return pProperties;
      }

      //------------------------------------------------------------------------
      //! Get the job results
      //------------------------------------------------------------------------
      PropertyList *GetResults()
      {
        return pResults;
      }

      //------------------------------------------------------------------------
      //! Get source
      //------------------------------------------------------------------------
      const URL &GetSource() const
      {
        return pSource;
      }

      //------------------------------------------------------------------------
      //! Get target
      //------------------------------------------------------------------------
      const URL &GetTarget() const
      {
        return pTarget;
      }

    protected:
      PropertyList *pProperties;
      PropertyList *pResults;
      URL           pSource;
      URL           pTarget;
  };
}

#endif // __XRD_CL_COPY_JOB_HH__

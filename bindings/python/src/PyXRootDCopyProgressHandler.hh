//------------------------------------------------------------------------------
// Copyright (c) 2012-2014 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
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

#ifndef PYXROOTD_COPY_PROGRESS_HANDLER_HH_
#define PYXROOTD_COPY_PROGRESS_HANDLER_HH_

#include "PyXRootD.hh"
#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClPropertyList.hh"
#include "XrdCl/XrdClURL.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Interface for copy progress notification
  //----------------------------------------------------------------------------
  class CopyProgressHandler : public XrdCl::CopyProgressHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CopyProgressHandler( PyObject *handler ) : handler( handler ) {}

      //------------------------------------------------------------------------
      //! Notify when a new job is about to start
      //------------------------------------------------------------------------
      virtual void BeginJob( uint16_t          jobNum,
                             uint16_t          jobTotal,
                             const XrdCl::URL *source,
                             const XrdCl::URL *target );

      //------------------------------------------------------------------------
      //! Notify when the previous job has finished
      //------------------------------------------------------------------------
      virtual void EndJob( uint16_t                   jobNum,
                           const XrdCl::PropertyList *result );

      //------------------------------------------------------------------------
      //! Notify about the progress of the current job
      //------------------------------------------------------------------------
      virtual void JobProgress( uint16_t jobNum,
                                uint64_t bytesProcessed,
                                uint64_t bytesTotal );

      //------------------------------------------------------------------------
      //! Determine whether the job should be canceled
      //------------------------------------------------------------------------
      virtual bool ShouldCancel(uint16_t jobNum);

    public:
      PyObject *handler;
  };
}
#endif /* PYXROOTD_COPY_PROGRESS_HANDLER_HH_ */

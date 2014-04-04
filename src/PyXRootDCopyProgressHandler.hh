//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
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

#ifndef PYXROOTD_COPY_PROGRESS_HANDLER_HH_
#define PYXROOTD_COPY_PROGRESS_HANDLER_HH_

#include "PyXRootD.hh"
#include "XrdCl/XrdClCopyProcess.hh"
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
      virtual void EndJob( const XrdCl::PropertyList *result );

      //------------------------------------------------------------------------
      //! Notify about the progress of the current job
      //------------------------------------------------------------------------
      virtual void JobProgress( uint64_t bytesProcessed, uint64_t bytesTotal );

      //------------------------------------------------------------------------
      //! Determine whether the job should be canceled
      //------------------------------------------------------------------------
      virtual bool ShouldCancel();

    public:
      PyObject *handler;
  };
}
#endif /* PYXROOTD_COPY_PROGRESS_HANDLER_HH_ */

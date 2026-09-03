//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
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

#ifndef __XRD_CL_THIRD_PARTY_COPY_PLUG_IN_HH__
#define __XRD_CL_THIRD_PARTY_COPY_PLUG_IN_HH__

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdOuc/XrdOucCompiler.hh"

#include <cstddef>
#include <ctime>
#include <string>

namespace XrdCl
{
  class PropertyList;
  class URL;

  //----------------------------------------------------------------------------
  //! Handle the progress of an asynchronous operation
  //----------------------------------------------------------------------------
  class ProgressHandler
  {
    public:
      virtual ~ProgressHandler() = default;

      //------------------------------------------------------------------------
      //! Called when the associated operation makes progress
      //!
      //! @param processed number of bytes processed until the moment of the call
      //! @param total     total number of bytes, zero if the operation does not
      //!                  know it
      //------------------------------------------------------------------------
      virtual void HandleProgress( std::size_t processed,
                                   std::size_t total = 0 ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Interface for plug-ins able to run a third party copy
  //!
  //! A filesystem plug-in advertises the capability by inheriting this class
  //! in addition to FileSystemPlugIn; it is discovered with a dynamic_cast.
  //----------------------------------------------------------------------------
  class ThirdPartyCopyPlugIn
  {
    public:
      virtual ~ThirdPartyCopyPlugIn();

      //------------------------------------------------------------------------
      //! Third party copy
      //!
      //! The copy runs to completion before this method returns.
      //!
      //! @param source           the file to be copied from
      //! @param dest             the location the file is copied to
      //! @param properties       properties of the copy, none if null
      //! @param progress_handler handler notified while the copy runs, none if
      //!                         null
      //! @param timeout          timeout value, if 0 the environment default
      //!                         is used
      //! @return                 status of the operation
      //------------------------------------------------------------------------
      virtual XRootDStatus ThirdPartyCopy( const std::string  &source,
                                           const std::string  &dest,
                                           const PropertyList *properties,
                                           ProgressHandler    *progress_handler,
                                           time_t              timeout ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Run a third party copy through the plug-in registered for the source URL
  //!
  //! Dispatches to the ThirdPartyCopyPlugIn interface of the filesystem
  //! plug-in the plug-in manager provides for the source protocol.
  //!
  //! @return errNotSupported if no plug-in handles the source URL or the
  //!         plug-in does not support third party copy
  //----------------------------------------------------------------------------
  XRootDStatus ThirdPartyCopy( const URL          &source,
                               const URL          &dest,
                               const PropertyList *properties,
                               ProgressHandler    *progress_handler,
                               time_t              timeout = 0 )
                               XRD_WARN_UNUSED_RESULT;
}

#endif // __XRD_CL_THIRD_PARTY_COPY_PLUG_IN_HH__

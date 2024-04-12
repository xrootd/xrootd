//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#ifndef SRC_XRDCL_XRDCLZIPLISTHANDLER_HH_
#define SRC_XRDCL_XRDCLZIPLISTHANDLER_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include <string>
#include <memory>

namespace XrdCl
{

  //----------------------------------------------------------------------------
  // DirList : Handle not a directory error
  //----------------------------------------------------------------------------
  class ZipListHandler : public ResponseHandler
  {

      //------------------------------------------------------------------------
      //! Possible steps in ZIP listing
      //!  - STAT  : stat the URL
      //!  - OPEN  : open the ZIP archive
      //!  - CLOSE : close the ZIP archive
      //1  - DONE  : we are done
      //------------------------------------------------------------------------
      enum Steps
      {
        STAT    = 0,
        OPEN    = 1,
        CLOSE   = 2,
        DONE    = 4
      };

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url     : endpoint URL
      //! @param path    : path to the ZIP
      //! @param flags   : listing flags
      //! @param handler : the original response handler
      //! @param timeout : operation timeout
      //------------------------------------------------------------------------
      ZipListHandler( const URL           &url,
                      const std::string   &path,
                      DirListFlags::Flags  flags,
                      ResponseHandler     *handler,
                      time_t               timeout = 0 ) :
        pUrl( url ), pFlags( flags ), pHandler( handler ),
        pTimeout( timeout ), pStartTime( time( 0 ) ), pStep( STAT )
      {
        if( !pTimeout )
        {
          int val = DefaultRequestTimeout;
          DefaultEnv::GetEnv()->GetInt( "RequestTimeout", val );
          pTimeout = val;
        }

        pUrl.SetPath( path );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~ZipListHandler()
      {

      }

      //------------------------------------------------------------------------
      //! Handle the server response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *statusptr,
                                   XrdCl::AnyObject    *responseptr );
    private:

      //------------------------------------------------------------------------
      //! Do normal listing if it is a directory (and not a ZIP archive)
      //------------------------------------------------------------------------
      void DoDirList( time_t timeLeft );

      //------------------------------------------------------------------------
      //! Open the ZIP archive
      //------------------------------------------------------------------------
      void DoZipOpen( time_t timeLeft );

      //------------------------------------------------------------------------
      //! Close the ZIP archive
      //------------------------------------------------------------------------
      void DoZipClose( time_t timeLeft );

      URL                             pUrl;
      DirListFlags::Flags             pFlags;
      ResponseHandler                *pHandler;
      time_t                          pTimeout;

      std::unique_ptr<DirectoryList>  pDirList;
      time_t                          pStartTime;

      File                            pFile;
      ZipArchive                      pZip;

      int                             pStep;

  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLZIPLISTHANDLER_HH_ */

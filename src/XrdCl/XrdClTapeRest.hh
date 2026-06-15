/******************************************************************************/
/*                                                                            */
/*                    X r d C l T a p e R e s t . h h                         */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#ifndef __XRD_CL_TAPE_REST_HH__
#define __XRD_CL_TAPE_REST_HH__

#include "XrdCl/XrdClXRootDResponses.hh"

#include <string>
#include <vector>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Locality values returned by the WLCG Tape REST API archiveinfo endpoint.
  //----------------------------------------------------------------------------
  enum class TapeRestLocality
  {
    Disk,
    Tape,
    DiskAndTape,
    Lost,
    None,
    Unavailable,
    Unknown
  };

  //----------------------------------------------------------------------------
  //! Client options used by TapeRestClient.
  //----------------------------------------------------------------------------
  struct TapeRestOptions
  {
    int timeout = -1;
    std::string cert;
    std::string key;
    unsigned int verbosity = 0;
  };

  //----------------------------------------------------------------------------
  //! Endpoint selected from the Tape REST API discovery document.
  //----------------------------------------------------------------------------
  struct TapeRestEndpoint
  {
    std::string uri;
    std::string version;
    std::string sitename;
  };

  //----------------------------------------------------------------------------
  //! Per-path archiveinfo response.
  //----------------------------------------------------------------------------
  struct TapeRestArchiveInfo
  {
    std::string url;
    std::string path;
    TapeRestLocality locality = TapeRestLocality::Unknown;
    std::string error;
  };

  //----------------------------------------------------------------------------
  //! Small synchronous client for the WLCG Tape REST API.
  //----------------------------------------------------------------------------
  class TapeRestClient
  {
    public:
      explicit TapeRestClient( const TapeRestOptions &options = TapeRestOptions() );
      ~TapeRestClient();

      XRootDStatus Discover( const std::string &url,
                             TapeRestEndpoint &endpoint ) const;

      XRootDStatus ArchiveInfo( const std::vector<std::string> &urls,
                                std::vector<TapeRestArchiveInfo> &results ) const;

      static std::string LocalityToString( TapeRestLocality locality );
      static TapeRestLocality LocalityFromString( const std::string &locality );

    private:
      TapeRestOptions pOptions;
  };
}

#endif // __XRD_CL_TAPE_REST_HH__

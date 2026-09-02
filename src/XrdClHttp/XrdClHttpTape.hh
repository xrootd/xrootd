/******************************************************************************/
/*                                                                            */
/*                    X r d C l H t t p T a p e . h h                         */
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

#ifndef XRDCLHTTP_TAPE_HH
#define XRDCLHTTP_TAPE_HH

#include "XrdClHttpVerb.hh"

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include <string>
#include <memory>
#include <vector>

#include <curl/curl.h>

namespace XrdClHttp
{
  struct TapeHttpRequest
  {
    HttpVerb method = HttpVerb::GET;
    std::string url;
    std::string body;
  };

  // Translates one XrdCl Prepare or Query operation into the two HTTP
  // requests used by Tape REST: endpoint discovery followed by the operation.
  // Transport remains owned by CurlTapeOp and the XrdClHttp worker pool.
  class TapeOperation
  {
    public:
      TapeOperation( const std::string &url,
                     const std::vector<std::string> &fileList,
                     XrdCl::PrepareFlags::Flags flags );
      TapeOperation( const std::string &url,
                     XrdCl::QueryCode::Code queryCode,
                     const XrdCl::Buffer &arg );
      ~TapeOperation();

      TapeOperation( const TapeOperation & ) = delete;
      TapeOperation &operator=( const TapeOperation & ) = delete;

      XrdCl::XRootDStatus Start( TapeHttpRequest &request );

      XrdCl::XRootDStatus Advance( CURL *curl,
                                  long statusCode,
                                  const std::string &body,
                                  TapeHttpRequest &request,
                                  std::string &response,
                                  bool &complete );

    private:
      struct Impl;
      std::unique_ptr<Impl> pImpl;
  };

  std::string TapeProblemResponse( long statusCode,
                                   const std::string &body );
}

#endif // XRDCLHTTP_TAPE_HH

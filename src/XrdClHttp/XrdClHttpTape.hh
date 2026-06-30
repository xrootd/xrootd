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

#ifndef __XRD_CL_HTTP_TAPE_HH__
#define __XRD_CL_HTTP_TAPE_HH__

#include "XrdCl/XrdClXRootDResponses.hh"

#include <array>
#include <string>
#include <vector>

namespace XrdClHttp
{
  XrdCl::XRootDStatus TapeDiscover( const std::string &url,
                                    int timeout,
                                    std::string &uri,
                                    std::string &version,
                                    std::string &sitename );

  XrdCl::XRootDStatus TapeStage(
    const std::string &url,
    const std::vector<std::array<std::string, 4>> &files,
    int timeout,
    std::string &requestId );

  XrdCl::XRootDStatus TapeStageStatus( const std::string &url,
                                       const std::string &requestId,
                                       int timeout,
                                       std::string &responseJson );

  XrdCl::XRootDStatus TapeStageCancel(
      const std::string &url,
      const std::string &requestId,
      const std::vector<std::string> &paths,
      int timeout );

  XrdCl::XRootDStatus TapeStageDelete( const std::string &url,
                                       const std::string &requestId,
                                       int timeout );

  XrdCl::XRootDStatus TapeRelease( const std::string &url,
                                   const std::string &requestId,
                                   const std::vector<std::string> &paths,
                                   int timeout );

  XrdCl::XRootDStatus TapeArchiveInfo(
    const std::vector<std::string> &urls,
    int timeout,
    std::string &responseJson );
}

#endif // __XRD_CL_HTTP_TAPE_HH__

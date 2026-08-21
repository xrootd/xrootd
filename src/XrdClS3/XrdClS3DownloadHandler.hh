/******************************************************************************/
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XrdClS3 client plugin for XRootD.                 */
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

#ifndef XRDCLS3_S3DOWNLOADHANDLER_HH
#define XRDCLS3_S3DOWNLOADHANDLER_HH

#include "XrdClS3Filesystem.hh"

#include <XrdCl/XrdClXRootDResponses.hh>

namespace XrdClS3 {

XrdCl::XRootDStatus DownloadUrl(const std::string &url, XrdClHttp::HeaderCallout *header_callout, XrdCl::ResponseHandler *handler, time_t timeout);

} // namespace XrdClS3
#endif // XRDCLS3_S3DOWNLOADHANDLER_HH
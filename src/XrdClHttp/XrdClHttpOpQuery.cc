/******************************************************************************/
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
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

#include "XrdClHttpOps.hh"

#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClLog.hh>

using namespace XrdClHttp;

void CurlQueryOp::Success()
{
    SetDone(false);
    m_logger->Debug(kLogXrdClHttp, "CurlQueryOp::Success");

    if (m_queryCode == XrdCl::QueryCode::XAttr) {
        XrdCl::Buffer *qInfo = new XrdCl::Buffer();
        qInfo->FromString(m_headers.GetETag());
        auto obj = new XrdCl::AnyObject();
        obj->Set(qInfo);

        m_handler->HandleResponse(new XrdCl::XRootDStatus(), obj);
        m_handler = nullptr;
    }
    else {
        m_logger->Error(kLogXrdClHttp, "Invalid information query type code");
        Fail(XrdCl::errInvalidArgs, XrdCl::errErrorResponse, "Unsupported query code");
    }
}

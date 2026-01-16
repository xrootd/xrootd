/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
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

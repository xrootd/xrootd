/***************************************************************
 *
 * Copyright (C) 2025, Pelican Project, Morgridge Institute for Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "XrdClCurlOps.hh"

#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClLog.hh>

using namespace XrdClCurl;

void CurlQueryOp::Success()
{
    SetDone(false);
    m_logger->Debug(kLogXrdClCurl, "CurlQueryOp::Success");

    if (m_queryCode == XrdCl::QueryCode::XAttr) {
        XrdCl::Buffer *qInfo = new XrdCl::Buffer();
        qInfo->FromString(m_headers.GetETag());
        auto obj = new XrdCl::AnyObject();
        obj->Set(qInfo);

        m_handler->HandleResponse(new XrdCl::XRootDStatus(), obj);
        m_handler = nullptr;
    }
    else {
        m_logger->Error(kLogXrdClCurl, "Invalid information query type code");
        Fail(XrdCl::errInvalidArgs, XrdCl::errErrorResponse, "Unsupported query code");
    }
}

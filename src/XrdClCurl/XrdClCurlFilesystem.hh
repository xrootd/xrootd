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

#ifndef XRDCLCURL_FILESYSTEM_HH
#define XRDCLCURL_FILESYSTEM_HH

#include "XrdClCurlConnectionCallout.hh"
#include "XrdClCurlHeaderCallout.hh"

#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClPlugInInterface.hh>
#include <XrdCl/XrdClURL.hh>

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace XrdCl {

class Log;

}

namespace XrdClCurl {

class HandlerQueue;

class Filesystem final : public XrdCl::FileSystemPlugIn {
public:
#if HAVE_XRDCL_IFACE6
    using timeout_t = time_t;
#else
    using timeout_t = uint16_t;
#endif

    Filesystem(const std::string &, std::shared_ptr<HandlerQueue> queue, XrdCl::Log *log);

    virtual ~Filesystem() noexcept;

    XrdCl::XRootDStatus DirList(const std::string          &path,
                                XrdCl::DirListFlags::Flags  flags,
                                XrdCl::ResponseHandler     *handler,
                                timeout_t                   timeout) override;

    virtual bool GetProperty(const std::string &name,
        std::string &value) const override;

    virtual XrdCl::XRootDStatus Locate(const std::string        &path,
                                       XrdCl::OpenFlags::Flags   flags,
                                       XrdCl::ResponseHandler   *handler,
                                       timeout_t                 timeout) override;

    virtual XrdCl::XRootDStatus MkDir(const std::string        &path,
                                      XrdCl::MkDirFlags::Flags  flags,
                                      XrdCl::Access::Mode       mode,
                                      XrdCl::ResponseHandler   *handler,
                                      timeout_t                 timeout) override;

    virtual XrdCl::XRootDStatus Rm(const std::string      &path,
                                   XrdCl::ResponseHandler *handler,
                                   timeout_t               timeout) override;

    virtual XrdCl::XRootDStatus RmDir(const std::string      &path,
                                      XrdCl::ResponseHandler *handler,
                                      timeout_t               timeout) override;

    virtual bool SetProperty(const std::string &name,
                             const std::string &value) override;

    virtual XrdCl::XRootDStatus Stat(const std::string      &path,
                                     XrdCl::ResponseHandler *handler,
                                     timeout_t               timeout) override;

    virtual XrdCl::XRootDStatus Query(XrdCl::QueryCode::Code  queryCode,
                                      const XrdCl::Buffer     &arg,
                                      XrdCl::ResponseHandler  *handler,
                                      timeout_t                timeout) override;

private:
    // Return a function pointer to the connection callout
    // Returns nullptr if this file isn't using the callout
    CreateConnCalloutType GetConnCallout() const;

    // The "*Response" variant of the callback response objects defined in DirectorCacheResponse.hh
    // are opt-in; if the caller isn't expecting them, then they will leak memory.  This
    // function determines whether the opt-in is enabled.
    bool SendResponseInfo() const;

    // Return the current computed URL to use for HTTP requests, provided the path
    //
    // Potentially the user-provided URL plus extra query parameters from the Filesystem properties.
    std::string GetCurrentURL(const std::string &path) const;

    // Protects the contents of m_properties
    mutable std::shared_mutex m_properties_mutex;

    std::shared_ptr<HandlerQueue> m_queue;
    std::atomic<XrdClCurl::HeaderCallout *> m_header_callout{};
    XrdCl::Log *m_logger{nullptr};
    XrdCl::URL m_url;
    std::unordered_map<std::string, std::string> m_properties;
};

} 

#endif // XRDCLCURL_FILESYSTEM_HH

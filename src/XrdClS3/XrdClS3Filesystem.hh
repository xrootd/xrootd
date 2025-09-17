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

#ifndef XRDCLS3_S3FILESYSTEM_HH
#define XRDCLS3_S3FILESYSTEM_HH

#include "../XrdClCurl/XrdClCurlHeaderCallout.hh"

#include <XrdCl/XrdClPlugInInterface.hh>

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace XrdCl {

class Log;

}

namespace XrdClS3 {

class Filesystem final : public XrdCl::FileSystemPlugIn {
public:
#if HAVE_XRDCL_IFACE6
    using timeout_t = time_t;
#else
    using timeout_t = uint16_t;
#endif

    Filesystem(const std::string &, XrdCl::Log *log);

    virtual ~Filesystem() noexcept;

    virtual XrdCl::XRootDStatus DirList(const std::string          &path,
                                        XrdCl::DirListFlags::Flags  flags,
                                        XrdCl::ResponseHandler     *handler,
                                        timeout_t                   timeout) override;

    virtual bool GetProperty(const std::string &name,
                             std::string       &value) const override;

    virtual XrdCl::XRootDStatus Locate(const std::string        &path,
                                       XrdCl::OpenFlags::Flags   flags,
                                       XrdCl::ResponseHandler   *handler,
                                       timeout_t                 timeout) override;

    virtual XrdCl::XRootDStatus MkDir(const std::string        &path,
                                      XrdCl::MkDirFlags::Flags  flags,
                                      XrdCl::Access::Mode       mode,
                                      XrdCl::ResponseHandler   *handler,
                                      timeout_t                 timeout) override;

    virtual XrdCl::XRootDStatus Query(XrdCl::QueryCode::Code  queryCode,
                                      const XrdCl::Buffer     &arg,
                                      XrdCl::ResponseHandler  *handler,
                                      timeout_t                timeout) override;

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

private:
    // State indicating whether the file is open.
    bool m_is_opened{false};

    // Given a path, provide the corresponding HTTP filesystem handle.
    std::pair<XrdCl::XRootDStatus, XrdCl::FileSystem*> GetFSHandle(const std::string &path);

    // Logger object for the filesystem
    XrdCl::Log *m_logger{nullptr};

    // The pelican://-URL represented by this filesystem object.
    XrdCl::URL m_url;

    // Properties set/get on this filesystem
    std::unordered_map<std::string, std::string> m_properties;

    // Protects the m_properties data from concurrent access
    mutable std::mutex m_properties_mutex;

    // Protects the m_handles data from concurrent access
    std::shared_mutex m_handles_mutex;

    // HTTPS handles for the corresponding endpoints.
    mutable std::unordered_map<std::string, XrdCl::FileSystem*> m_handles;

    // Class for setting up the required HTTP headers for S3 requests
    class S3HeaderCallout : public XrdClCurl::HeaderCallout {
    public:
        S3HeaderCallout(Filesystem &fs) : m_parent(fs)
        {}

        virtual ~S3HeaderCallout() noexcept = default;

        virtual std::shared_ptr<HeaderList> GetHeaders(const std::string &verb,
                                                       const std::string &url,
                                                       const HeaderList &headers) override;

    private:
        Filesystem &m_parent;
    };

    S3HeaderCallout m_header_callout{*this};
};

}

#endif // XRDCLS3_S3FILESYSTEM_HH

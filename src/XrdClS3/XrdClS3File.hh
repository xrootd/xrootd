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

#ifndef XRDCLS3_S3FILE_HH
#define XRDCLS3_S3FILE_HH

#include "../XrdClCurl/XrdClCurlHeaderCallout.hh"

#include <XrdCl/XrdClFile.hh>

namespace XrdCl {

class Log;

}

namespace XrdClS3 {

class File final : public XrdCl::FilePlugIn {
public:
    File(XrdCl::Log *log);

#if HAVE_XRDCL_IFACE6
    using timeout_t = time_t;
#else
    using timeout_t = uint16_t;
#endif

    virtual ~File() noexcept;

    virtual XrdCl::XRootDStatus Close(XrdCl::ResponseHandler *handler,
                                     timeout_t                timeout) override;

    virtual bool GetProperty( const std::string &name,
                            std::string &value ) const override;

    virtual bool IsOpen() const override;

    virtual XrdCl::XRootDStatus Open(const std::string      &url,
                                     XrdCl::OpenFlags::Flags flags,
                                     XrdCl::Access::Mode     mode,
                                     XrdCl::ResponseHandler *handler,
                                     timeout_t               timeout) override;

    virtual XrdCl::XRootDStatus PgRead(uint64_t                offset,
                                       uint32_t                size,
                                       void                   *buffer,
                                       XrdCl::ResponseHandler *handler,
                                       timeout_t               timeout) override;

    virtual XrdCl::XRootDStatus Read(uint64_t                offset,
                                     uint32_t                size,
                                     void                   *buffer,
                                     XrdCl::ResponseHandler *handler,
                                     timeout_t               timeout) override;

    virtual bool SetProperty( const std::string &name,
                            const std::string &value ) override;

    virtual XrdCl::XRootDStatus Stat(bool                    force,
                                     XrdCl::ResponseHandler *handler,
                                     timeout_t               timeout) override;

    virtual XrdCl::XRootDStatus VectorRead(const XrdCl::ChunkList &chunks,
                                           void                   *buffer,
                                           XrdCl::ResponseHandler *handler,
                                           timeout_t               timeout ) override;

    virtual XrdCl::XRootDStatus Write(uint64_t            offset,
                                  uint32_t                size,
                                  const void             *buffer,
                                  XrdCl::ResponseHandler *handler,
                                  timeout_t               timeout) override;

    virtual XrdCl::XRootDStatus Write(uint64_t             offset,
                                  XrdCl::Buffer          &&buffer,
                                  XrdCl::ResponseHandler  *handler,
                                  timeout_t                timeout) override;

private:
    bool m_is_opened{false};

    // The flags used to open the file
    XrdCl::OpenFlags::Flags m_open_flags{XrdCl::OpenFlags::None};

    std::string m_url;
    XrdCl::Log *m_logger{nullptr};
    mutable std::mutex m_properties_mutex;
    std::unordered_map<std::string, std::string> m_properties;

    std::unique_ptr<XrdCl::File> m_wrapped_file;

    // Given a path, provide the corresponding HTTP file handle.
    std::tuple<XrdCl::XRootDStatus, std::string, XrdCl::File*> GetFileHandle(const std::string &url);

    // Class for setting up the required HTTP headers for S3 requests
    class S3HeaderCallout : public XrdClCurl::HeaderCallout {
    public:
        S3HeaderCallout(File &fs) : m_parent(fs)
        {}

        virtual ~S3HeaderCallout() noexcept = default;

        virtual std::shared_ptr<HeaderList> GetHeaders(const std::string &verb,
                                                       const std::string &url,
                                                       const HeaderList &headers) override;

    private:
        File &m_parent;
    };

    S3HeaderCallout m_header_callout{*this};
};

}

#endif // XRDCLS3_S3FILE_HH

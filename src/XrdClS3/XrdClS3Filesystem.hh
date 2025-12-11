/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#ifndef XRDCLS3_S3FILESYSTEM_HH
#define XRDCLS3_S3FILESYSTEM_HH

#include "../XrdClHttp/XrdClHttpHeaderCallout.hh"

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
    Filesystem(const std::string &, XrdCl::Log *log);

    virtual ~Filesystem() noexcept;

    virtual XrdCl::XRootDStatus DirList(const std::string          &path,
                                        XrdCl::DirListFlags::Flags  flags,
                                        XrdCl::ResponseHandler     *handler,
                                        time_t                      timeout) override;

    virtual bool GetProperty(const std::string &name,
                             std::string       &value) const override;

    virtual XrdCl::XRootDStatus Locate(const std::string        &path,
                                       XrdCl::OpenFlags::Flags   flags,
                                       XrdCl::ResponseHandler   *handler,
                                       time_t                    timeout) override;

    virtual XrdCl::XRootDStatus MkDir(const std::string        &path,
                                      XrdCl::MkDirFlags::Flags  flags,
                                      XrdCl::Access::Mode       mode,
                                      XrdCl::ResponseHandler   *handler,
                                      time_t                    timeout) override;

    virtual XrdCl::XRootDStatus Query(XrdCl::QueryCode::Code  queryCode,
                                      const XrdCl::Buffer     &arg,
                                      XrdCl::ResponseHandler  *handler,
                                      time_t                   timeout) override;

    virtual XrdCl::XRootDStatus Rm(const std::string      &path,
                                   XrdCl::ResponseHandler *handler,
                                   time_t                  timeout) override;

    virtual XrdCl::XRootDStatus RmDir(const std::string      &path,
                                      XrdCl::ResponseHandler *handler,
                                      time_t                  timeout) override;

    virtual bool SetProperty(const std::string &name,
                             const std::string &value) override;

    virtual XrdCl::XRootDStatus Stat(const std::string      &path,
                                     XrdCl::ResponseHandler *handler,
                                     time_t                  timeout) override;

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
    class S3HeaderCallout : public XrdClHttp::HeaderCallout {
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

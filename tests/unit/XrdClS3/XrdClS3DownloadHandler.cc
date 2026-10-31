/******************************************************************************/
/* Copyright (C) 2026, Pelican Project, Morgridge Institute for Research      */
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

#include "XrdClS3/XrdClS3DownloadHandler.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClMessageUtils.hh>
#include <XrdCl/XrdClPlugInInterface.hh>
#include <XrdCl/XrdClPlugInManager.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

// File plug-in that opens successfully then fails the first Read()
// synchronously (without invoking the response handler). That hits the
// ownership path fixed for #2790 in S3DownloadHandler.
class FailReadFile : public XrdCl::FilePlugIn {
public:
    FailReadFile() = default;
    virtual ~FailReadFile() = default;

    virtual XrdCl::XRootDStatus Open(const std::string & /*url*/,
                                     XrdCl::OpenFlags::Flags /*flags*/,
                                     XrdCl::Access::Mode /*mode*/,
                                     XrdCl::ResponseHandler *handler,
                                     time_t /*timeout*/) override
    {
        m_is_open = true;
        if (handler) {
            handler->HandleResponse(new XrdCl::XRootDStatus(), nullptr);
        }
        return XrdCl::XRootDStatus();
    }

    virtual XrdCl::XRootDStatus Close(XrdCl::ResponseHandler *handler,
                                      time_t /*timeout*/) override
    {
        m_is_open = false;
        if (handler) {
            handler->HandleResponse(new XrdCl::XRootDStatus(), nullptr);
        }
        return XrdCl::XRootDStatus();
    }

    virtual XrdCl::XRootDStatus Read(uint64_t /*offset*/,
                                     uint32_t /*size*/,
                                     void * /*buffer*/,
                                     XrdCl::ResponseHandler * /*handler*/,
                                     time_t /*timeout*/) override
    {
        // Synchronous failure: do not call the handler.
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errErrorResponse, 0,
                                   "injected sync read failure");
    }

    virtual bool IsOpen() const override { return m_is_open; }

    virtual bool SetProperty(const std::string & /*name*/,
                             const std::string & /*value*/) override
    {
        return true;
    }

private:
    bool m_is_open{false};
};

class FailReadFactory : public XrdCl::PlugInFactory {
public:
    virtual ~FailReadFactory() = default;

    virtual XrdCl::FilePlugIn *CreateFile(const std::string & /*url*/) override
    {
        return new FailReadFile();
    }

    virtual XrdCl::FileSystemPlugIn *CreateFileSystem(const std::string & /*url*/) override
    {
        return nullptr;
    }
};

} // namespace

class DownloadHandlerFixture : public testing::Test {
protected:
    void SetUp() override
    {
        auto *factory = new FailReadFactory();
        XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(factory);
    }

    void TearDown() override
    {
        XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(nullptr);
    }
};

// Ensure a synchronous Read failure after a successful Open does not crash
// (use-after-move of the parent handler) and still reports an error to the
// caller.
TEST_F(DownloadHandlerFixture, SyncReadFailurePropagates)
{
    XrdCl::SyncResponseHandler handler;
    auto st = XrdClS3::DownloadUrl("failread://localhost/obj", nullptr, &handler, time_t{10});
    ASSERT_TRUE(st.IsOK()) << "Open should succeed: " << st.ToString();

    handler.WaitForResponse();
    auto *status = handler.GetStatus();
    ASSERT_TRUE(status != nullptr);
    ASSERT_FALSE(status->IsOK()) << "Expected read failure to reach the user handler";
    EXPECT_NE(std::string::npos, status->GetErrorMessage().find("injected sync read failure"))
        << status->ToString();
}

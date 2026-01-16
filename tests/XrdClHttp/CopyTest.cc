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

#include "XrdClHttp/XrdClHttpFactory.hh"
#include "XrdClHttp/XrdClHttpFile.hh"
#include "XrdClHttp/XrdClHttpUtil.hh"
#include "XrdClHttp/XrdClHttpWorker.hh"
#include "../XrdClHttpCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>

#include <gtest/gtest.h>

class CurlCopyFixture : public TransferFixture {
public:
    void SetUp() {
        TransferFixture::SetUp();
        m_factory.reset(new XrdClHttp::Factory());
    }

protected:
    // Factory object; creating one will initialize the worker threads
    std::unique_ptr<XrdClHttp::Factory> m_factory;
};

TEST_F(CurlCopyFixture, Test)
{
    auto source_url = GetOriginURL() + "/test/source_file";
    WritePattern(source_url, 2*1024, 'a', 1023);

    auto dest_url = GetOriginURL() + "/test/dest_file";
    XrdClHttp::CurlCopyOp::Headers source_headers;
    source_headers.emplace_back("Authorization", "Bearer " + GetReadToken());
    XrdClHttp::CurlCopyOp::Headers dest_headers;
    dest_headers.emplace_back("Authorization", "Bearer " + GetWriteToken());
    SyncResponseHandler srh;
    auto logger = XrdCl::DefaultEnv::GetLog();
    logger->Debug(XrdClHttp::kLogXrdClHttp, "About to start copy operation");
    std::unique_ptr<XrdClHttp::CurlCopyOp> op(new XrdClHttp::CurlCopyOp(
        &srh, source_url, source_headers, dest_url, dest_headers, {10, 0}, logger, nullptr
    ));

    // We must create at least one file or filesystem object for the factory to initialize
    // itself; after that, we call into its internals directly.
    auto fs = m_factory->CreateFileSystem("https://example.com");
    delete fs;

    m_factory->Produce(std::move(op));
    logger->Debug(XrdClHttp::kLogXrdClHttp, "Will wait on copy operation");
    srh.Wait();
    logger->Debug(XrdClHttp::kLogXrdClHttp, "Copy operation complete");
    auto [status, obj] = srh.Status();
    ASSERT_TRUE(status->IsOK()) << "Copy command failed with error: " << status->ToString();

    VerifyContents(dest_url, 2*1024, 'a', 1023);
}

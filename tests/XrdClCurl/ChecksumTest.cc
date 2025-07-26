/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClCurl client plugin for XRootD.               */
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

#include "XrdClCurl/XrdClCurlFactory.hh"
#include "XrdClCurl/XrdClCurlFile.hh"
#include "../XrdClCurlCommon/TransferTest.hh"

#include <gtest/gtest.h>
#include <XrdCl/XrdClBuffer.hh>

#include <deque>
#include <fstream>

class CurlChecksumFixture : public TransferFixture {
protected:
    void WriteString(const std::string &name, const std::string &contents);
    void VerifyString(const std::string &name, const std::string &contents);

    void TearDown() override;

private:
    void PrintFileEnd(const std::string &log_file_path);
};

void
CurlChecksumFixture::PrintFileEnd(const std::string &log_file_path)
{
    // Read and print last 500 lines of the server log
    std::ifstream log_file(log_file_path);

    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path << std::endl;
    } else {
        std::deque<std::string> lines;
        std::string line;
        while (std::getline(log_file, line)) {
            lines.push_back(line);
            if (lines.size() > 500) {
                lines.pop_front();
            }
        }

        std::cerr << "\n--- Last 500 lines of " << log_file_path << " ---\n";
        for (const auto &line : lines) {
            std::cerr << line << std::endl;
        }
        std::cerr << "--------------------------------------------------\n";
    }
}

void
CurlChecksumFixture::TearDown()
{
    TransferFixture::TearDown();
    if (::testing::Test::HasFailure()) {
        PrintFileEnd("../curl/origin.log");
        PrintFileEnd("../curl/cache.log");
    }
}

void
CurlChecksumFixture::WriteString(const std::string &name, const std::string &contents)
{
    XrdCl::File fh;

    auto url = name + "?authz=" + GetWriteToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Write, XrdCl::Access::Mode(0755), static_cast<XrdClCurl::File::timeout_t>(0));
    ASSERT_TRUE(rv.IsOK()) << "Failed to open " << name << " for write: " << rv.ToString();

    rv = fh.Write(0, contents.size(), contents.data(), static_cast<XrdClCurl::File::timeout_t>(0));
    ASSERT_TRUE(rv.IsOK()) << "Failed to write " << name << ": " << rv.ToString();

    rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());

    VerifyString(name, contents);
}

void
CurlChecksumFixture::VerifyString(const std::string &name, const std::string &contents)
{
    XrdCl::File fh;

    auto url = name + "?authz=" + GetReadToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<XrdClCurl::File::timeout_t>(0));
    ASSERT_TRUE(rv.IsOK()) << "Failed to open " << name << " for read: " << rv.ToString();

    uint32_t bytesRead;
    std::string result;
    result.resize(contents.size());
    rv = fh.Read(0, contents.size(), result.data(), bytesRead, static_cast<XrdClCurl::File::timeout_t>(0));
    ASSERT_TRUE(rv.IsOK()) << "Failed to read " << name << ": " << rv.ToString();
    ASSERT_EQ(contents.size(), bytesRead);

    ASSERT_EQ(result, contents);
}

TEST_F(CurlChecksumFixture, Basic)
{
    auto source_url = std::string("/test/checksum_md5");
    WritePattern(GetOriginURL() + source_url, 2*1024, 'a', 1023);

    // MD5 hand-calculated:
    // >>> o = hashlib.md5()
    // >>> o.update(("a"*1023).encode())
    // >>> o.update(("b"*1023).encode())
    // >>> o.update("cc".encode())
    // >>> o.hexdigest()
    // '4a42dabcf0c6233b3dac41196313e748'
    const std::string expected_response = "md5 4a42dabcf0c6233b3dac41196313e748";

    // To query the checksum, we use the Checksum query code and put the desired
    // URL in the buffer.  The preferred checksum type is specified in the URL
    // query string by the parameter `cks.type`.
    //
    // The expected response is the checksum type followed by the checksum value.
    XrdCl::FileSystem fs{XrdCl::URL(GetCacheURL())};

    XrdCl::Buffer buffer;
    buffer.FromString(source_url + "?cks.type=md5&directread&authz=" + GetReadToken());
    SyncResponseHandler srh;
    auto st = fs.Query(XrdCl::QueryCode::Checksum, buffer, &srh, 0);
    ASSERT_TRUE(st.IsOK());
    srh.Wait();
    auto [status, obj] = srh.Status();
    ASSERT_EQ(status->IsOK(), true) << "MD5 checksum query failed with " << status->ToString();
    XrdCl::Buffer *resp{nullptr};
    obj->Get(resp);

    ASSERT_EQ(resp->ToString(), expected_response);

    source_url = "/test/checksum_crc32c";
    WriteString(GetOriginURL() + source_url, "dog");
    
    // From https://www.iana.org/assignments/http-dig-alg/http-dig-alg.xhtml, the
    // crc32c of the string `dog` should be 0a72a4df.
    buffer.FromString(source_url + "?cks.type=crc32c&directread&authz=" + GetReadToken());
    auto status3 = fs.Query(XrdCl::QueryCode::Checksum, buffer, &srh, 0);
    ASSERT_TRUE(status3.IsOK());
    srh.Wait();
    auto [status2, obj2] = srh.Status();
    ASSERT_EQ(status2->IsOK(), true);
    resp = nullptr;
    obj2->Get(resp);
    // NOTE: As of 18 March 2025, `pelican` binaries do not enable crc32c.  Patch sent 
    // in https://github.com/PelicanPlatform/pelican/pull/2106
    // Uncomment when the corresponding version is widely available.
    // ASSERT_EQ(resp->ToString(), "crc32c 0a72a4df");

    source_url = "/test/checksum_md5";
    buffer.FromString(source_url + "?cks.type=md5&directread&authz=" + GetReadToken());
    status3 = fs.Query(XrdCl::QueryCode::Checksum, buffer, &srh, 0);
    ASSERT_TRUE(status3.IsOK());
    srh.Wait();
    std::tie(status2, obj2) = srh.Status();
    ASSERT_EQ(status2->IsOK(), true);
}

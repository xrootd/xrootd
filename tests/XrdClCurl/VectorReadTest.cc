/****************************************************************
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

#include "XrdClCurl/XrdClCurlOps.hh"
#include "XrdClCurl/XrdClCurlFile.hh"
#include "../XrdClCurlCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>
#include <XrdCl/XrdClLog.hh>

#include <gtest/gtest.h>

class CurlVectorFixture : public TransferFixture {};

TEST_F(CurlVectorFixture, Test)
{
    auto url = GetOriginURL() + "/test/vector_read_file";
    WritePattern(url, 8, 'a', 2); // Results in file with content pattern of aabbccddeeff..

    XrdCl::File fh;

    url += "?authz=" + GetReadToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<XrdClCurl::File::timeout_t>(10));
    ASSERT_TRUE(rv.IsOK());
        
    std::vector<char> a; a.resize(2);
    std::vector<char> b; b.resize(2);
    std::vector<char> c; c.resize(2);

    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());
    chunks.emplace_back(4, 2, c.data());

    XrdCl::VectorReadInfo *vrInfo{nullptr};
    rv = fh.VectorRead(chunks, nullptr, vrInfo, static_cast<XrdClCurl::File::timeout_t>(10));
    ASSERT_TRUE(rv.IsOK());
    ASSERT_NE(vrInfo, nullptr);
    std::unique_ptr<XrdCl::VectorReadInfo> vrInfoPtr(vrInfo);

    ASSERT_EQ(vrInfo->GetSize(), 6);
    ASSERT_EQ(vrInfo->GetChunks().size(), 3);
    for (int idx=0; idx<3; idx++) {
        ASSERT_EQ(vrInfo->GetChunks()[idx].GetOffset(), idx * 2);
        ASSERT_EQ(vrInfo->GetChunks()[idx].GetLength(), 2);
    }
    ASSERT_EQ(a[0], 'a');
    ASSERT_EQ(a[1], 'a');
    ASSERT_EQ(b[0], 'b');
    ASSERT_EQ(b[1], 'b');
    ASSERT_EQ(c[0], 'c');
    ASSERT_EQ(c[1], 'c');

    rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());
}

TEST_F(CurlVectorFixture, WriteTest)
{
    auto logger = XrdCl::DefaultEnv::GetLog();

    std::vector<char> a; a.resize(2);
    std::vector<char> b; b.resize(2);
    std::vector<char> c; c.resize(2);
    std::vector<char> d; d.resize(2);

    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());
    chunks.emplace_back(4, 2, c.data());

    XrdClCurl::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr.SetStatusCode(200);
    char response[] = "aabbccdd";
    auto rv = vr.Write(response, 8);
    ASSERT_EQ(rv, 8);
    ASSERT_EQ(a[0], 'a');
    ASSERT_EQ(a[1], 'a');
    ASSERT_EQ(b[0], 'b');
    ASSERT_EQ(b[1], 'b');
    ASSERT_EQ(c[0], 'c');
    ASSERT_EQ(c[1], 'c');

    a[0] = a[1] = '\0';
    b[0] = b[1] = '\0';
    chunks.clear();
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());
    chunks.emplace_back(6, 2, d.data());

    XrdClCurl::CurlVectorReadOp vr2(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr2.SetStatusCode(200);
    rv = vr2.Write(response, 8);
    ASSERT_EQ(rv, 8);
    ASSERT_EQ(a[0], 'a');
    ASSERT_EQ(a[1], 'a');
    ASSERT_EQ(b[0], 'b');
    ASSERT_EQ(b[1], 'b');
    ASSERT_EQ(d[0], 'd');
    ASSERT_EQ(d[1], 'd');

    a[0] = a[1] = '\0';
    b[0] = b[1] = '\0';
    d[0] = d[1] = '\0';
    XrdClCurl::CurlVectorReadOp vr3(nullptr, "https://example.com", {0, 0}, chunks, logger, nullptr, nullptr);
    vr3.SetStatusCode(206);
    vr3.SetSeparator("123456");
    char response2[] =
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 0-1/8\r\n"
        "\r\n"
        "aa"
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 2-3/8\r\n"
        "\r\n"
        "bb"
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 6-7/8\r\n"
        "\r\n"
        "dd"
        "\r\n--123456--\r\n";
    rv = vr3.Write(response2, strlen(response2));
    ASSERT_EQ(rv, strlen(response2));
    ASSERT_EQ(a[0], 'a');
    ASSERT_EQ(a[1], 'a');
    ASSERT_EQ(b[0], 'b');
    ASSERT_EQ(b[1], 'b');
    ASSERT_EQ(d[0], 'd');
    ASSERT_EQ(d[1], 'd');
}

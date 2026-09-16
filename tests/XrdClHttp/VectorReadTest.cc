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

#include "XrdClHttp/XrdClHttpOps.hh"
#include "XrdClHttp/XrdClHttpFile.hh"
#include "../XrdClHttpCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>
#include <XrdCl/XrdClLog.hh>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

class CurlVectorFixture : public TransferFixture {};

TEST_F(CurlVectorFixture, Test)
{
    auto url = GetOriginURL() + "/test/vector_read_file";
    WritePattern(url, 8, 'a', 2); // Results in file with content pattern of aabbccddeeff..

    XrdCl::File fh;

    url += "?authz=" + GetReadToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<time_t>(10));
    ASSERT_TRUE(rv.IsOK());
        
    std::vector<char> a; a.resize(2);
    std::vector<char> b; b.resize(2);
    std::vector<char> c; c.resize(2);

    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());
    chunks.emplace_back(4, 2, c.data());

    XrdCl::VectorReadInfo *vrInfo{nullptr};
    rv = fh.VectorRead(chunks, nullptr, vrInfo, static_cast<time_t>(10));
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

    XrdClHttp::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
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

    XrdClHttp::CurlVectorReadOp vr2(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
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
    XrdClHttp::CurlVectorReadOp vr3(nullptr, "https://example.com", {0, 0}, chunks, logger, nullptr, nullptr);
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

// Multipart header lines may be split across successive curl write callbacks.
// CurlVectorReadOp::Write() must reassemble them via m_response_headers and then
// clear that buffer.  Feed the first Content-Range across three writes so the
// middle chunk never completes a header line (no '\r\n' in that piece).
TEST_F(CurlVectorFixture, WriteTestSplitMultipartHeaderAcrossWrites)
{
    auto logger = XrdCl::DefaultEnv::GetLog();

    std::vector<char> a(2);
    std::vector<char> b(2);

    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());

    XrdClHttp::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr.SetStatusCode(206);
    vr.SetSeparator("123456");

    const char *part1 =
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: by";
    const char *part2 = "tes 0-";
    const char *part3 =
        "1/8\r\n"
        "\r\n"
        "aa"
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 2-3/8\r\n"
        "\r\n"
        "bb"
        "\r\n--123456--\r\n";

    ASSERT_EQ(strlen(part1), vr.Write(const_cast<char *>(part1), strlen(part1)));
    ASSERT_EQ(strlen(part2), vr.Write(const_cast<char *>(part2), strlen(part2)));
    ASSERT_EQ(strlen(part3), vr.Write(const_cast<char *>(part3), strlen(part3)));

    ASSERT_EQ('a', a[0]);
    ASSERT_EQ('a', a[1]);
    ASSERT_EQ('b', b[0]);
    ASSERT_EQ('b', b[1]);
}

// Curl often ends a write after the Content-Range line, with the blank line that
// terminates the part headers plus the payload in the next write.  If that blank
// line is copied as payload, the next "boundary" is binary data.
TEST_F(CurlVectorFixture, WriteTestSplitAfterContentRangeBeforeBody)
{
    auto logger = XrdCl::DefaultEnv::GetLog();

    std::vector<char> a(2);
    std::vector<char> b(2);

    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());

    XrdClHttp::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr.SetStatusCode(206);
    vr.SetSeparator("123456");

    const char *part1 =
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 0-1/8\r\n";
    const char *part2 =
        "\r\n"
        "aa"
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 2-3/8\r\n"
        "\r\n"
        "bb"
        "\r\n--123456--\r\n";

    ASSERT_EQ(strlen(part1), vr.Write(const_cast<char *>(part1), strlen(part1)));
    ASSERT_EQ(strlen(part2), vr.Write(const_cast<char *>(part2), strlen(part2)));

    ASSERT_EQ('a', a[0]);
    ASSERT_EQ('a', a[1]);
    ASSERT_EQ('b', b[0]);
    ASSERT_EQ('b', b[1]);
}

// CRLF of the boundary marker split across writes: "...--123456\r" then
// "\nContent-type: ...".  find("\r\n") in the second buffer matches the
// Content-type line ending, gluing both lines together unless the straddling
// \r\n is recognized.
TEST_F(CurlVectorFixture, WriteTestSplitCRLFAfterBoundary)
{
    auto logger = XrdCl::DefaultEnv::GetLog();

    std::vector<char> a(2);
    std::vector<char> b(2);

    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, 2, a.data());
    chunks.emplace_back(2, 2, b.data());

    XrdClHttp::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr.SetStatusCode(206);
    vr.SetSeparator("123456");

    const char *part1 = "\r\n--123456\r";
    const char *part2 =
        "\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 0-1/8\r\n"
        "\r\n"
        "aa"
        "\r\n--123456\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n"
        "Content-Range: bytes 2-3/8\r\n"
        "\r\n"
        "bb"
        "\r\n--123456--\r\n";

    ASSERT_EQ(strlen(part1), vr.Write(const_cast<char *>(part1), strlen(part1)));
    ASSERT_EQ(strlen(part2), vr.Write(const_cast<char *>(part2), strlen(part2)));

    ASSERT_EQ('a', a[0]);
    ASSERT_EQ('a', a[1]);
    ASSERT_EQ('b', b[0]);
    ASSERT_EQ('b', b[1]);
}

namespace {

constexpr const char *kBoundary = "123456";

struct Part {
    off_t offset;
    std::string payload;
};

std::string MakeXrdHttpPart(off_t start, std::string_view payload, off_t filesize)
{
    const off_t end = start + static_cast<off_t>(payload.size()) - 1;
    std::string part;
    part += "\r\n--";
    part += kBoundary;
    part += "\r\n";
    part += "Content-type: text/plain; charset=UTF-8\r\n";
    part += "Content-Range: bytes ";
    part += std::to_string(start);
    part += "-";
    part += std::to_string(end);
    part += "/";
    part += std::to_string(filesize);
    part += "\r\n\r\n";
    part.append(payload.data(), payload.size());
    return part;
}

std::string MakeXrdHttpMultipart(const std::vector<Part> &parts, off_t filesize)
{
    std::string body;
    for (const auto &part : parts) {
        body += MakeXrdHttpPart(part.offset, part.payload, filesize);
    }
    body += "\r\n--";
    body += kBoundary;
    body += "--\r\n";
    return body;
}

void ExpectMultipartBody(const std::string &body, const std::vector<Part> &expected)
{
    std::vector<std::vector<char>> bufs;
    bufs.reserve(expected.size());
    XrdCl::ChunkList chunks;
    chunks.reserve(expected.size());
    for (const auto &part : expected) {
        bufs.emplace_back(part.payload.size(), '\0');
        chunks.emplace_back(part.offset, static_cast<uint32_t>(part.payload.size()), bufs.back().data());
    }

    auto logger = XrdCl::DefaultEnv::GetLog();
    XrdClHttp::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr.SetStatusCode(206);
    vr.SetSeparator(kBoundary);

    ASSERT_EQ(body.size(), vr.Write(const_cast<char *>(body.data()), body.size()));
    for (size_t i = 0; i < expected.size(); ++i) {
        ASSERT_EQ(expected[i].payload, std::string(bufs[i].data(), bufs[i].size()));
    }
}

} // namespace

// Server emits parts in offset order; the client request list is shuffled.
TEST_F(CurlVectorFixture, WriteTestOutOfOrderChunks)
{
    const std::vector<Part> requested{
        {6, "dd"},
        {0, "aa"},
        {2, "bb"},
    };
    const std::vector<Part> server_parts{
        {0, "aa"},
        {2, "bb"},
        {6, "dd"},
    };
    ExpectMultipartBody(MakeXrdHttpMultipart(server_parts, 8), requested);
}

// Extra CRLFs before boundaries, as permitted by RFC 7233 and emitted by XrdHttp.
TEST_F(CurlVectorFixture, WriteTestExtraCRLFBeforeBoundary)
{
    const std::vector<Part> parts{
        {0, "aa"},
        {2, "bb"},
        {6, "dd"},
    };
    std::string body = "\r\n\r\n";
    for (const auto &part : parts) {
        body += MakeXrdHttpPart(part.offset, part.payload, 8);
    }
    body += "\r\n\r\n--";
    body += kBoundary;
    body += "--\r\n";
    ExpectMultipartBody(body, parts);
}

// Non-multipart 206: raw body, no boundary framing. GetOffset() defaults to 0.
TEST_F(CurlVectorFixture, WriteTestSingleRange206)
{
    const std::string payload = "aabbccdd";
    auto logger = XrdCl::DefaultEnv::GetLog();
    std::vector<char> buf(payload.size(), '\0');
    XrdCl::ChunkList chunks;
    chunks.emplace_back(0, static_cast<uint32_t>(payload.size()), buf.data());

    XrdClHttp::CurlVectorReadOp vr(nullptr, "https://example.com", {10, 0}, chunks, logger, nullptr, nullptr);
    vr.SetStatusCode(206);
    ASSERT_EQ(payload.size(), vr.Write(const_cast<char *>(payload.data()), payload.size()));
    ASSERT_EQ(payload, std::string(buf.data(), buf.size()));
}

// Server coalesced two adjacent requested ranges into one Content-Range.
TEST_F(CurlVectorFixture, WriteTestCoalescedRange)
{
    const std::vector<Part> requested{
        {0, "aa"},
        {2, "bb"},
    };
    std::string body = MakeXrdHttpPart(0, "aabb", 8);
    body += "\r\n--";
    body += kBoundary;
    body += "--\r\n";
    ExpectMultipartBody(body, requested);
}

// Payload bytes that look like framing (\r\n, --boundary, NULs) must be copied as data
TEST_F(CurlVectorFixture, WriteTestBinaryPayloadLooksLikeFraming)
{
    std::string p0(16, '\0');
    p0[0] = '\r';
    p0[1] = '\n';
    p0.replace(2, 8, "--123456");
    p0[10] = '\xff';
    p0[11] = '\r';
    p0[12] = '\n';
    p0.replace(13, 3, "xyz");

    std::string p1(16, '\xaa');
    p1.replace(0, 10, "--123456--");
    p1[10] = '\r';
    p1[11] = '\n';
    p1[12] = '\0';
    p1[13] = '\xff';

    const std::vector<Part> parts{
        {0, std::move(p0)},
        {100, std::move(p1)},
    };
    ExpectMultipartBody(MakeXrdHttpMultipart(parts, 1024), parts);
}

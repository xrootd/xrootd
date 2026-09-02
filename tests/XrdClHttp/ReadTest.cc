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
#include "XrdClHttp/XrdClHttpHeaderCallout.hh"
#include "XrdClHttp/XrdClHttpWorker.hh"
#include "../XrdClHttpCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>

#include <gtest/gtest.h>

#include <charconv>
#include <memory>

class CurlReadFixture : public TransferFixture {
};

// Read a single file
//
// Because all reads happen in serial and linearly, this will test the prefetch
// capabilities
TEST_F(CurlReadFixture, SerialTest)
{
    auto chunk_ctr = 10;
    auto url = GetOriginURL() + "/test/read_single_" + std::to_string(chunk_ctr);
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, chunk_ctr * 100'000, 'a', chunk_ctr * 10'000));
}

// Ensure that curl reads operate after the prefetch times out.
TEST_F(CurlReadFixture, PrefetchTimeoutTest)
{
    auto chunk_ctr = 10;
    auto chunk_size = chunk_ctr * 10'000;
    auto file_size = chunk_ctr * 100'000;
    char starting_char = 'a';
    auto url = GetOriginURL() + "/test/read_prefetch_" + std::to_string(chunk_ctr);
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, file_size, starting_char, chunk_size));

    XrdCl::File fh;
    url += "?authz=" + GetReadToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<time_t>(0));
    ASSERT_TRUE(rv.IsOK());
    fh.SetProperty("XrdClHttpMaintenancePeriod", "1");
    fh.SetProperty("XrdClHttpStallTimeout", "500ms");

    // Submit multiple reads, one after another.
    size_t read_size = (chunk_size >= file_size)
                                    ? file_size
                                    : chunk_size;
    unsigned char curChunkByte = starting_char;
    off_t offset = 0;
    std::vector<std::string> readBuffers;
    std::vector<std::unique_ptr<SyncResponseHandler>> handlers;
    size_t sizeToRead = read_size;
    size_t expectedSize = file_size;
    while (sizeToRead) {
        readBuffers.emplace_back(sizeToRead, curChunkByte - 1);
        handlers.emplace_back(new SyncResponseHandler());
        auto rv = fh.Read(offset, sizeToRead, readBuffers.back().data(), handlers.back().get(), static_cast<time_t>(0));
        ASSERT_TRUE(rv.IsOK());

        expectedSize -= sizeToRead;
        offset += sizeToRead;
        sizeToRead = (static_cast<size_t>(chunk_size) >= expectedSize)
                                                       ? expectedSize
                                                       : chunk_size;
        curChunkByte += 1;

        if (handlers.size() == 2) {
            std::string value;
            ASSERT_TRUE(fh.GetProperty("IsPrefetching", value));
            ASSERT_EQ(value, "true");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else if (handlers.size() == 3) {
            std::string value;
            ASSERT_TRUE(fh.GetProperty("IsPrefetching", value));
            ASSERT_EQ(value, "false");
        }
    }

    // Wait on each of the reads, verify the correct response.
    sizeToRead = read_size;
    expectedSize = file_size;
    int idx = 0;
    offset = 0;
    curChunkByte = starting_char;
    while (sizeToRead) {
        auto &handler = handlers[idx];
        idx++;
        handler->Wait();
        fprintf(stderr, "Checking result of read operation %d\n", idx);

        auto [status, obj] = handler->Status();
        ASSERT_TRUE(status);
        ASSERT_TRUE(status->IsOK()) << "Read operation failed with error: " << status->ToString();
        ASSERT_TRUE(obj);

        XrdCl::ChunkInfo *ci = nullptr;
        obj->Get(ci);
        ASSERT_TRUE(ci);

        ASSERT_EQ(sizeToRead, ci->GetLength());
        ASSERT_EQ(offset, ci->GetOffset());

        std::string correctBuffer(sizeToRead, curChunkByte);
        std::string readBuffer(static_cast<char *>(ci->GetBuffer()), ci->GetLength());
        ASSERT_EQ(readBuffer, correctBuffer);

        expectedSize -= sizeToRead;
        offset += sizeToRead;
        sizeToRead = (static_cast<size_t>(chunk_size) >= expectedSize)
                                                       ? expectedSize
                                                       : chunk_size;
        curChunkByte += 1;
    }

    rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());
}

// Read a single file concurrently
//
// All reads are submitted sequentially then waited upon; tests cases where
// multiple outstanding reads "stack up" in a prefetch-friendly way.
TEST_F(CurlReadFixture, ConcurrentTest)
{
    auto chunk_ctr = 10;
    auto chunk_size = chunk_ctr * 10'000;
    auto file_size = chunk_ctr * 100'000;
    char starting_char = 'a';
    auto url = GetOriginURL() + "/test/read_concurrent_" + std::to_string(chunk_ctr);
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, file_size, starting_char, chunk_size));

    XrdCl::File fh;
    url += "?authz=" + GetReadToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<time_t>(0));
    ASSERT_TRUE(rv.IsOK());

    // Submit multiple reads, one after another.
    size_t read_size = (static_cast<off_t>(chunk_size) >= file_size)
                                                        ? file_size
                                                        : chunk_size;
    unsigned char curChunkByte = starting_char;
    off_t offset = 0;
    std::vector<std::string> readBuffers;
    std::vector<std::unique_ptr<SyncResponseHandler>> handlers;
    size_t sizeToRead = read_size;
    size_t expectedSize = file_size;
    while (sizeToRead) {
        readBuffers.emplace_back(sizeToRead, curChunkByte - 1);
        handlers.emplace_back(new SyncResponseHandler());
        auto rv = fh.Read(offset, sizeToRead, readBuffers.back().data(), handlers.back().get(), static_cast<time_t>(0));
        ASSERT_TRUE(rv.IsOK());

        expectedSize -= sizeToRead;
        offset += sizeToRead;
        sizeToRead = (static_cast<size_t>(chunk_size) >= expectedSize)
                                                       ? expectedSize
                                                       : chunk_size;
        curChunkByte += 1;
    }

    // Wait on each of the reads, verify the correct response.
    sizeToRead = read_size;
    expectedSize = file_size;
    int idx = 0;
    offset = 0;
    curChunkByte = starting_char;
    while (sizeToRead) {
        auto &handler = handlers[idx];
        idx++;
        handler->Wait();
        fprintf(stderr, "Checking result of read operation %d\n", idx);

        auto [status, obj] = handler->Status();
        ASSERT_TRUE(status);
        ASSERT_TRUE(status->IsOK());
        ASSERT_TRUE(obj);

        XrdCl::ChunkInfo *ci = nullptr;
        obj->Get(ci);
        ASSERT_TRUE(ci);

        ASSERT_EQ(sizeToRead, ci->GetLength());
        ASSERT_EQ(offset, ci->GetOffset());

        std::string correctBuffer(sizeToRead, curChunkByte);
        std::string readBuffer(static_cast<char *>(ci->GetBuffer()), ci->GetLength());
        ASSERT_EQ(readBuffer, correctBuffer);

        expectedSize -= sizeToRead;
        offset += sizeToRead;
        sizeToRead = (static_cast<size_t>(chunk_size) >= expectedSize)
                                                       ? expectedSize
                                                       : chunk_size;
        curChunkByte += 1;
    }

    rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());
}

// Full-download against a chunked/unknown-size response must keep prefetch enabled.
//
// The origin returns Transfer-Encoding: chunked (no Content-Length) when the client
// sends TE: trailers and X-Transfer-Status: true. Combined with the full-download
// Range fix (no Range header), PrefetchSize becomes -1 and sequential reads still
// hit the ongoing prefetch stream.
TEST_F(CurlReadFixture, FullDownloadUnknownSizePrefetchTest)
{
    auto chunk_ctr = 10;
    auto chunk_size = chunk_ctr * 10'000;
    auto file_size = chunk_ctr * 100'000;
    char starting_char = 'a';
    auto url = GetOriginURL() + "/test/read_fulldownload_unknown_" + std::to_string(chunk_ctr);
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, file_size, starting_char, chunk_size));

    class TrailersCallout : public XrdClHttp::HeaderCallout {
    public:
        TrailersCallout(const std::string &token) : m_token(token) {}
        virtual ~TrailersCallout() = default;

        virtual std::shared_ptr<HeaderList> GetHeaders(const std::string & /*verb*/,
                                                       const std::string & /*url*/,
                                                       const HeaderList &input_headers) override
        {
            auto headers = std::make_shared<HeaderList>(input_headers);
            headers->emplace_back("Authorization", "Bearer " + m_token);
            headers->emplace_back("TE", "trailers");
            headers->emplace_back("X-Transfer-Status", "true");
            return headers;
        }

    private:
        std::string m_token;
    };

    TrailersCallout callout(GetReadToken());
    // Construct with URL so the HTTP client plug-in is loaded before SetProperty.
    XrdCl::File fh(url);

    auto callout_loc = reinterpret_cast<long long>(&callout);
    char callout_buf[16];
    auto result = std::to_chars(callout_buf, callout_buf + sizeof(callout_buf) - 1, callout_loc, 16);
    ASSERT_EQ(result.ec, std::errc{});
    std::string callout_str(callout_buf, result.ptr - callout_buf);
    ASSERT_TRUE(fh.SetProperty("XrdClHttpHeaderCallout", callout_str));
    ASSERT_TRUE(fh.SetProperty("XrdClHttpFullDownload", "true"));

    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<time_t>(0));
    ASSERT_TRUE(rv.IsOK()) << rv.ToString();

    std::string prefetch_size;
    ASSERT_TRUE(fh.GetProperty("XrdClHttpPrefetchSize", prefetch_size));
    ASSERT_EQ(prefetch_size, "-1") << "Expected unknown size under chunked full-download";

    std::string is_prefetching;
    ASSERT_TRUE(fh.GetProperty("IsPrefetching", is_prefetching));
    ASSERT_EQ(is_prefetching, "true");

    ASSERT_NO_FATAL_FAILURE(VerifyContents(fh, file_size, starting_char, chunk_size));
}

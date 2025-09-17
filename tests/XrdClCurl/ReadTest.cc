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
#include "XrdClCurl/XrdClCurlWorker.hh"
#include "../XrdClCurlCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>

#include <gtest/gtest.h>

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
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<XrdClCurl::File::timeout_t>(0));
    ASSERT_TRUE(rv.IsOK());
    fh.SetProperty("XrdClCurlMaintenancePeriod", "1");
    fh.SetProperty("XrdClCurlStallTimeout", "500ms");

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
        auto rv = fh.Read(offset, sizeToRead, readBuffers.back().data(), handlers.back().get(), static_cast<XrdClCurl::File::timeout_t>(0));
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
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<XrdClCurl::File::timeout_t>(0));
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
        auto rv = fh.Read(offset, sizeToRead, readBuffers.back().data(), handlers.back().get(), static_cast<XrdClCurl::File::timeout_t>(0));
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

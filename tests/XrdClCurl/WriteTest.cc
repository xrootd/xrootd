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

#include "XrdClCurl/XrdClCurlFile.hh"
#include "../XrdClCurlCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>

#include <gtest/gtest.h>

#include <thread>

class CurlWriteFixture : public TransferFixture {
public:

// Write 300 files in serial of differing size and contents.
//
// Goal is to find any bugs that don't require heavy concurrency to trigger
// Count of files selected to keep the test run quick (5s on a test laptop)
void InvokeMultipleWrites(const std::string &prefix, size_t size_count, size_t chunk_count) {
    for (unsigned size_ctr = 1; size_ctr <= size_count; size_ctr ++) {
        for (unsigned chunk_ctr = 1; chunk_ctr <= chunk_count; chunk_ctr ++) {
            auto url = GetOriginURL() + "/test/write_" + prefix + "_" + std::to_string(size_ctr) + "_" + std::to_string(chunk_ctr);
            ASSERT_NO_FATAL_FAILURE(WritePattern(url, chunk_ctr * 100'000, 'a', chunk_ctr * 10'000));
        }
    }
}
};

// Write 300 files in serial of differing size and contents.
//
// Goal is to find any bugs that don't require heavy concurrency to trigger
// Count of files selected to keep the test run quick (5s on a test laptop)
TEST_F(CurlWriteFixture, SerialTest)
{
    ASSERT_NO_FATAL_FAILURE(InvokeMultipleWrites("serial", 30, 10));
}

// Write 100 files per thread in 10 threads
//
// Goal is to trigger concurrency-related write bugs.
TEST_F(CurlWriteFixture, ParallelTest)
{
    std::vector<std::thread> threads;
    for (unsigned ctr=0; ctr<10; ctr++) {
        threads.emplace_back(&CurlWriteFixture::InvokeMultipleWrites, this, "parallel_" + std::to_string(ctr), 10, 10);
    }
    for (unsigned ctr=0; ctr<10; ctr++) {
        threads[ctr].join();
    }
}

// Ensure that writes fail after the PUT times out.
TEST_F(CurlWriteFixture, PutTimeoutTest)
{
    XrdCl::File fh;
    auto name = GetOriginURL() + "/test/write_timeout";
    auto url = name + "?authz=" + GetWriteToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Write, XrdCl::Access::Mode(0755), static_cast<XrdClCurl::File::timeout_t>(0));
    ASSERT_TRUE(rv.IsOK()) << "Failed to open " << name << " for write: " << rv.ToString();

    fh.SetProperty("XrdClCurlMaintenancePeriod", "1");
    fh.SetProperty("XrdClCurlStallTimeout", "100ms");

    uint32_t sizeToWrite = 10'000;
    std::string writeBuffer(sizeToWrite, 'a');
    uint64_t offset = 0;
    rv = fh.Write(offset, sizeToWrite, writeBuffer.data(), static_cast<XrdClCurl::File::timeout_t>(10));
    ASSERT_TRUE(rv.IsOK()) << "Failed to write " << name << ": " << rv.ToString();
    offset += sizeToWrite;

    std::this_thread::sleep_for(std::chrono::seconds(2));

    rv = fh.Write(offset, sizeToWrite, writeBuffer.data(), static_cast<XrdClCurl::File::timeout_t>(10));
    ASSERT_FALSE(rv.IsOK()) << "Succeeded writing to a file that should have timed out " << name << ": " << rv.ToString();
    fprintf(stderr, "Write failed with error message: %s\n", rv.ToStr().c_str());

    rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());
}

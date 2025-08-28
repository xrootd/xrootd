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

#include "../XrdClCurlCommon/TransferTest.hh"
#include "XrdClS3/XrdClS3DownloadHandler.hh"
#include "XrdClS3/XrdClS3Filesystem.hh"

#include <gtest/gtest.h>

#include <memory>

class S3ReadFixture : public TransferFixture {
public:
    void SetUp() override {
        TransferFixture::SetUp();
    }

protected:
    XrdClCurl::HeaderCallout *GetReadTokenCallout() {
        return &m_read_callout;
    }

private:
    class ReadAuthzCallout : public XrdClCurl::HeaderCallout {
        public:
            ReadAuthzCallout(const S3ReadFixture &parent) :
                m_parent(parent)
            {}

            virtual ~ReadAuthzCallout() = default;

            virtual std::shared_ptr<HeaderList> GetHeaders(const std::string & /*verb*/, const std::string & /*url*/, const HeaderList & input_headers) {
                std::shared_ptr<HeaderList> headers(new HeaderList(input_headers));
                headers->emplace_back("Authorization", m_parent.GetReadToken());
                return headers;
            }

        private:
            const S3ReadFixture &m_parent;
    };

    ReadAuthzCallout m_read_callout{*this};
};

// Read a single file
//
// Because all reads happen in serial and linearly, this will test the prefetch
// capabilities
TEST_F(S3ReadFixture, SerialTest)
{
    auto chunk_ctr = 10;
    auto url = GetCacheURL() + "/test-bucket/read_single_" + std::to_string(chunk_ctr);
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, chunk_ctr * 100'000, 'a', chunk_ctr * 10'000));
}

// Test for single-shot callback for downloading an entire URL into a buffer
TEST_F(S3ReadFixture, OneShotTest)
{
    constexpr unsigned chunk_ctr = 10;
    auto url = GetCacheURL() + "/test-bucket/read_singleshot";
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, chunk_ctr * 100'000, 'a', chunk_ctr * 10'000));

    std::unique_ptr<SyncResponseHandler> srh(new SyncResponseHandler());
    auto st = XrdClS3::DownloadUrl(url, GetReadTokenCallout(), srh.get(), XrdClS3::Filesystem::timeout_t{10});
    ASSERT_TRUE(st.IsOK());

    srh->Wait();
    auto [status, object] = srh->Status();
    ASSERT_TRUE(status != nullptr);
    ASSERT_TRUE(status->IsOK()) << "Failed to run full-download of object: " << status->ToString();
    ASSERT_TRUE(object != nullptr);
    XrdCl::Buffer *buffer = nullptr;
    object->Get(buffer);
    ASSERT_TRUE(buffer);
    ASSERT_EQ(buffer->GetSize(), chunk_ctr * 100'000);

    off_t chunkSize = chunk_ctr * 10'000;
    off_t expectedSize = chunk_ctr * 100'000;
    auto sizeToRead = (static_cast<off_t>(chunkSize) >= expectedSize)
                                        ? expectedSize
                                        : chunkSize;
    unsigned char curChunkByte = 'a';
    buffer->SetCursor(0);
    while (sizeToRead) {
        std::string readBuffer(buffer->GetBufferAtCursor(), chunk_ctr * 10'000);

        std::string correctBuffer(sizeToRead, curChunkByte);
        ASSERT_EQ(readBuffer, correctBuffer);

        expectedSize -= sizeToRead;
        buffer->AdvanceCursor(sizeToRead);
        sizeToRead = (static_cast<off_t>(chunkSize) >= expectedSize)
                                            ? expectedSize
                                            : chunkSize;
        curChunkByte += 1;
    }
}

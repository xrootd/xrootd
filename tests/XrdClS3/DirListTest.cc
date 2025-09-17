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

#include <XrdCl/XrdClFileSystem.hh>

#include <deque>
#include <fstream>

class S3ListFixture : public TransferFixture {
protected:

void TearDown() override {
    TransferFixture::TearDown();
    if (::testing::Test::HasFailure()) {
                // Read and print last 1000 lines of the server log
        const std::string log_file_path = "../s3/server.log";
        std::ifstream log_file(log_file_path);

        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file: " << log_file_path << std::endl;
        } else {
            std::deque<std::string> lines;
            std::string line;
            while (std::getline(log_file, line)) {
                lines.push_back(line);
                if (lines.size() > 1000) {
                    lines.pop_front();
                }
            }

            std::cerr << "\n--- Last 1000 lines of " << log_file_path << " ---\n";
            for (const auto &line : lines) {
                std::cerr << line << std::endl;
            }
            std::cerr << "--------------------------------------------------\n";
        }
    }
}

void SetupDirPattern(const std::string &name) {
    auto chunk_max = 10;
    for (auto chunk_ctr = 1; chunk_ctr < chunk_max; chunk_ctr++) {
        auto url = GetCacheURL() + "/test-bucket/list_fixture/" + name + "/subdir/file_1_" + std::to_string(chunk_ctr);
        ASSERT_NO_FATAL_FAILURE(WritePattern(url, chunk_ctr * 5'000, 'a', chunk_ctr * 1'000 + 1));
    }
    auto url = GetCacheURL() + "/test-bucket/list_fixture/" + name + "/parent_file_1_" + std::to_string(chunk_max);
    ASSERT_NO_FATAL_FAILURE(WritePattern(url, chunk_max * 5'000, 'a', chunk_max * 1'000 + 1));
}
};

TEST_F(S3ListFixture, SimpleStat)
{
    ASSERT_NO_FATAL_FAILURE(SetupDirPattern("simple_stat"));

    std::string fname = "/test-bucket/list_fixture/simple_stat/parent_file_1_10";
    XrdCl::FileSystem fs(GetCacheURL());

    XrdCl::StatInfo *response{nullptr};
    auto st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to stat new file: " << st.ToString();
    ASSERT_FALSE(response->GetFlags() & XrdCl::StatInfo::IsDir);
    ASSERT_TRUE(response->GetFlags() & XrdCl::StatInfo::IsReadable);
    ASSERT_EQ(response->GetSize(), 50'000);
    delete response;

    fname += "_does_not_exist";
    st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    ASSERT_FALSE(st.IsOK()) << "Expected error for file not existing";
    ASSERT_EQ(st.errNo, kXR_NotFound);
}

TEST_F(S3ListFixture, SimpleList)
{
    ASSERT_NO_FATAL_FAILURE(SetupDirPattern("simple_list"));

    std::string fname = "/test-bucket/list_fixture/simple_list/subdir";
    XrdCl::FileSystem fs(GetCacheURL());

    XrdCl::DirectoryList *response{nullptr};
    auto st = fs.DirList(fname + "?authz=" + GetReadToken(), XrdCl::DirListFlags::None, response, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to list directory: " << st.ToString();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->GetSize(), 9);
    for (unsigned idx = 1; idx < response->GetSize(); idx++) {
        auto li = response->At(idx - 1);
        ASSERT_NE(li, nullptr);
        ASSERT_EQ(li->GetName(), "file_1_" + std::to_string(idx));
        ASSERT_FALSE(li->GetStatInfo()->GetFlags() & XrdCl::StatInfo::IsDir);
        ASSERT_TRUE(li->GetStatInfo()->GetFlags() & XrdCl::StatInfo::IsReadable);
        ASSERT_EQ(li->GetStatInfo()->GetSize(), idx * 5'000);
    }
    delete response;

    fname += "_does_not_exist";
    st = fs.DirList(fname + "?authz=" + GetReadToken(), XrdCl::DirListFlags::None, response, 10);
    ASSERT_FALSE(st.IsOK()) << "Expected error for file not existing";
    ASSERT_EQ(st.errNo, kXR_NotFound);

    fname = "test-bucket/list_fixture/simple_list";

    response = nullptr;
    st = fs.DirList(fname + "?authz=" + GetReadToken(), XrdCl::DirListFlags::None, response, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to list directory: " << st.ToString();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->GetSize(), 2);

    auto li = response->At(0);
    ASSERT_NE(li, nullptr);
    ASSERT_EQ(li->GetName(), "parent_file_1_10");
    ASSERT_FALSE(li->GetStatInfo()->GetFlags() & XrdCl::StatInfo::IsDir);
    ASSERT_EQ(li->GetStatInfo()->GetSize(), 50'000);
    li = response->At(1);
    ASSERT_NE(li, nullptr);
    ASSERT_EQ(li->GetName(), "subdir");
    ASSERT_TRUE(li->GetStatInfo()->GetFlags() & XrdCl::StatInfo::IsDir);
    delete response;
}

TEST_F(S3ListFixture, Mkdir)
{
    XrdCl::FileSystem fs(GetCacheURL());

    std::string fname = "test-bucket/mkdir/foo";

    XrdCl::StatInfo *si = nullptr;
    auto st = fs.Stat(fname + "?authz=" + GetReadToken(), si, 10);
    ASSERT_FALSE(st.IsOK());
    ASSERT_EQ(st.errNo, kXR_NotFound);
    ASSERT_EQ(si, nullptr);

    st = fs.MkDir(fname + "?authz=" + GetWriteToken(), XrdCl::MkDirFlags::None, XrdCl::Access::Mode::None, 10);
    ASSERT_TRUE(st.IsOK());

    st = fs.Stat(fname + "?authz=" + GetReadToken(), si, 10);
    ASSERT_TRUE(st.IsOK());
    ASSERT_NE(si, nullptr);
    ASSERT_TRUE(si->GetFlags() & XrdCl::StatInfo::Flags::IsDir);
    delete si;

    st = fs.RmDir(fname + "?authz=" + GetWriteToken(), 10);
    ASSERT_TRUE(st.IsOK());

    si = nullptr;
    st = fs.Stat(fname + "?authz=" + GetReadToken(), si, 10);
    ASSERT_FALSE(st.IsOK());
    ASSERT_EQ(st.errNo, kXR_NotFound);
    ASSERT_EQ(si, nullptr);
}

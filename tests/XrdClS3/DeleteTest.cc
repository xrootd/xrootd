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

class S3DeleteFixture : public TransferFixture {};

TEST_F(S3DeleteFixture, Test)
{
    std::string fname = "/test-bucket/delete_file";
    auto url = GetCacheURL() + fname;
    WritePattern(url, 8, 'a', 2);
    XrdCl::FileSystem fs(GetCacheURL());

    XrdCl::StatInfo *response{nullptr};
    auto st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to stat new file: " << st.ToString();
    delete response;

    st = fs.Rm(fname + "?authz=" + GetWriteToken(), 10);
    ASSERT_TRUE(st.IsOK()) << "Failed to remove file: " << st.ToString();

    response = nullptr;
    st = fs.Stat(fname + "?authz=" + GetReadToken(), response, 10);
    ASSERT_FALSE(st.IsOK()) << "Stat of removed file should have failed";
    ASSERT_EQ(st.errNo, kXR_NotFound);
}
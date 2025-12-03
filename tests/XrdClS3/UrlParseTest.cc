/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClS3 client plugin for XRootD.                 */
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

#include "gtest/gtest.h"
#include "XrdClS3/XrdClS3Factory.hh"

#include <filesystem>

#include <fcntl.h>

using namespace XrdClS3;

class FactoryFixture : public testing::Test {
public:

void SetUp() override {
    auto temp_dir_path = std::filesystem::temp_directory_path() / "factory_creds.XXXXXX";
    m_tempdir = temp_dir_path.string();
    ASSERT_TRUE(mkdtemp(m_tempdir.data()) != NULL) << "Failed to create temporary directory for credentials";
};

void TearDown() override {
    if (!m_tempdir.empty()) {
        std::filesystem::remove_all(m_tempdir);
    }
};

std::string GetTempLocation(const std::string &name) const {
    return (std::filesystem::path(m_tempdir) / name).string();
}

bool WriteFile(const std::string &location, const std::string &contents) {
    std::filesystem::path path(m_tempdir);
    path /= location;
    int fd = open(path.c_str(), O_CREAT|O_TRUNC|O_WRONLY, 0600);
    if (fd == -1) return false;

	auto ptr = &contents[0];
	ssize_t nwrite;
	auto nleft = contents.size();

	while (nleft > 0) {
	REISSUE_WRITE:
		nwrite = write(fd, ptr, nleft);
		if (nwrite < 0) {
			if (errno == EINTR) {
				goto REISSUE_WRITE;
			}
			close(fd);
			return false;
		}

		nleft -= nwrite;
		ptr += nwrite;
	}

	close(fd);
	return true;
}

bool WriteCreds(const std::string &access_key_location, const std::string &access_key, const std::string &secret_key_location, const std::string &secret_key)
{
    if (!WriteFile(access_key_location, access_key)) return false;

    return WriteFile(secret_key_location, secret_key);
}

private:
    std::string m_tempdir;
};

TEST(Factory, ExtractHostname) {
    std::string_view url = "https://bucket-name.s3.amazonaws.com/path/to/object";
    std::string_view hostname = Factory::ExtractHostname(url);
    ASSERT_EQ(hostname, "bucket-name.s3.amazonaws.com");

    url = "https://s3.amazonaws.com";
    hostname = Factory::ExtractHostname(url);
    ASSERT_EQ(hostname, "s3.amazonaws.com");

    url = "s3://p0@localhost:55708/";
    hostname = Factory::ExtractHostname(url);
    ASSERT_EQ(hostname, "localhost");

    url = "https://user:foo@s3.amazonaws.com:9443/";
    hostname = Factory::ExtractHostname(url);
    ASSERT_EQ(hostname, "s3.amazonaws.com");

    url = "https://user:foo@s3.amazonaws.com:9443/foo/bar";
    hostname = Factory::ExtractHostname(url);
    ASSERT_EQ(hostname, "s3.amazonaws.com");

    url = "s3://s3.amazonaws.com?foo=bar";
    hostname = Factory::ExtractHostname(url);
    ASSERT_STREQ(std::string(hostname).c_str(), "s3.amazonaws.com");

    url = "s3.amazonaws.com?foo=bar";
    hostname = Factory::ExtractHostname(url);
    ASSERT_EQ(hostname, "");
}

TEST(Factory, GenerateHttpUrl) {
    Factory::SetEndpoint("s3.amazonaws.com");
    Factory::SetRegion("us-east-1");
    Factory::SetUrlStyle("virtual");

    std::string s3_url = "s3://bucket-name/path/to/object";
    std::string err_msg;
    std::string https_url;
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://bucket-name.us-east-1.s3.amazonaws.com/path/to/object");

    Factory::SetEndpoint("");
    s3_url = "s3://p0@localhost:55708/bucket-name/object";
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://bucket-name.us-east-1.localhost:55708/object");

    Factory::SetEndpoint("s3.amazonaws.com");
    s3_url = "s3://bucket-name/path/to/object?versionId=12345";
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://bucket-name.us-east-1.s3.amazonaws.com/path/to/object?versionId=12345");

    s3_url = "s3://bucket-name.us-east-1.s3.amazonaws.com/path/to/object";
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://bucket-name.us-east-1.s3.amazonaws.com/path/to/object");
    
    s3_url = "s3://s3.amazonaws.com/bucket-name/path/to/object";
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://bucket-name.us-east-1.s3.amazonaws.com/path/to/object");

    Factory::SetEndpoint("");
    s3_url = "s3://s3.amazonaws.com/bucket-name/path";
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://bucket-name.us-east-1.s3.amazonaws.com/path");

    Factory::SetUrlStyle("path");
    s3_url = "s3://s3.amazonaws.com/bucket-name/path";
    ASSERT_TRUE(Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg));
    ASSERT_TRUE(err_msg.empty());
    ASSERT_EQ(https_url, "https://s3.amazonaws.com/bucket-name/path");
}

TEST(Factory, GetBucketFromHttpsUrl) {
    std::string bucket_name;
    Factory::SetEndpoint("s3.amazonaws.com");
    Factory::SetRegion("us-east-1");
    Factory::SetUrlStyle("virtual");

    bucket_name = Factory::GetBucketFromHttpsUrl("https://bucket-name.us-east-1.s3.amazonaws.com/path/to/object");
    ASSERT_EQ(bucket_name, "bucket-name");

    bucket_name = Factory::GetBucketFromHttpsUrl("https://bucket-name.s3.amazonaws.com/path/to/object");
    ASSERT_EQ(bucket_name, "bucket-name");

    Factory::SetRegion("");
    bucket_name = Factory::GetBucketFromHttpsUrl("https://bucket-name.s3.amazonaws.com/path/to/object");
    ASSERT_EQ(bucket_name, "bucket-name");

    Factory::SetUrlStyle("path");
    bucket_name = Factory::GetBucketFromHttpsUrl("https://s3.amazonaws.com/bucket-name/path/to/object");
    ASSERT_EQ(bucket_name, "bucket-name");
}

TEST_F(FactoryFixture, GetCredentialsForBucket) {

    WriteCreds("access_key.txt", "my_access_key",
               "secret_key.txt", "my_secret_key");
    Factory::SetDefaultCredentials(GetTempLocation("access_key.txt"), GetTempLocation("secret_key.txt"));
    std::string err_msg;
    auto [access, secret, ok] = Factory::GetCredentialsForBucket("bucket-name", err_msg);
    ASSERT_TRUE(ok) << "Failure to get credentials for bucket: " << err_msg;
    ASSERT_EQ(access, "my_access_key");
    ASSERT_EQ(secret, "my_secret_key");

    WriteCreds("access_key.bucket.txt", "my_access_key.bucket",
               "secret_key.bucket.txt", "my_secret_key.bucket");

    Factory::SetBucketCredentials("bucket-name", GetTempLocation("access_key.bucket.txt"), GetTempLocation("secret_key.bucket.txt"));

    std::tie(access, secret, ok) = Factory::GetCredentialsForBucket("bucket-name2", err_msg);
    ASSERT_TRUE(ok);
    ASSERT_EQ(access, "my_access_key");
    ASSERT_EQ(secret, "my_secret_key");

    std::tie(access, secret, ok) = Factory::GetCredentialsForBucket("bucket-name", err_msg);
    ASSERT_TRUE(ok);
    ASSERT_EQ(access, "my_access_key"); // Due to caching, should get the old value not the new one.
    ASSERT_EQ(secret, "my_secret_key");

    Factory::SetBucketCredentials("bucket-name3", GetTempLocation("access_key.bucket.txt"), GetTempLocation("secret_key.bucket.txt"));
    std::tie(access, secret, ok) = Factory::GetCredentialsForBucket("bucket-name3", err_msg);
    ASSERT_TRUE(ok);
    ASSERT_EQ(access, "my_access_key.bucket");
    ASSERT_EQ(secret, "my_secret_key.bucket");

    Factory::SetBucketCredentials("bucket-name4", GetTempLocation("access_key.bucket2.txt"), GetTempLocation("secret_key.bucket2.txt"));

    std::tie(access, secret, ok) = Factory::GetCredentialsForBucket("bucket-name4", err_msg);
    ASSERT_FALSE(ok);
}

TEST_F(FactoryFixture, GenerateV4Signature) {
    Factory::ResetCredCache();
    Factory::SetEndpoint("s3.amazonaws.com");
    Factory::SetRegion("us-east-1");
    Factory::SetUrlStyle("virtual");

    WriteCreds("access_key.txt", "AKIAIOSFODNN7EXAMPLE", "secret_key.txt", "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
    Factory::SetDefaultCredentials(GetTempLocation("access_key.txt"), GetTempLocation("secret_key.txt"));

    // Examples come from https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Range", "bytes=0-9"},
        {"x-amz-content-sha256", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"x-amz-date", "20130524T000000Z"}
    };
    std::string auth_token, err_msg;
    ASSERT_TRUE(Factory::GenerateV4Signature("https://examplebucket.s3.amazonaws.com/test.txt", "GET", headers, auth_token, err_msg));
    ASSERT_EQ(err_msg, "");
    ASSERT_EQ(auth_token, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=host;range;x-amz-content-sha256;x-amz-date,Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41");

    headers = {
        {"x-amz-content-sha256", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"x-amz-date", "20130524T000000Z"}
    };
    ASSERT_TRUE(Factory::GenerateV4Signature("https://examplebucket.s3.amazonaws.com?lifecycle", "GET", headers, auth_token, err_msg));
    ASSERT_EQ(err_msg, "");
    ASSERT_EQ(auth_token, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=fea454ca298b7da1c68078a5d1bdbfbbe0d65c699e0f91ac7a200a0136783543");

    ASSERT_TRUE(Factory::GenerateV4Signature("https://examplebucket.s3.amazonaws.com?max-keys=2&prefix=J", "GET", headers, auth_token, err_msg));
    ASSERT_EQ(err_msg, "");
    ASSERT_EQ(auth_token, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=34b48302e7b5fa45bde8084f4b7868a86f0a534bc59db6670ed5711ef69dc6f7");

    headers = {
        {"Date", "Fri, 24 May 2013 00:00:00 GMT"},
        {"x-amz-date", "20130524T000000Z"},
        {"x-amz-storage-class", "REDUCED_REDUNDANCY"},
        {"x-amz-content-sha256", "44ce7dd67c959e0d3524ffac1771dfbba87d2b6b4b4e99e42034a8b803f8b072"}
    };
    ASSERT_TRUE(Factory::GenerateV4Signature("https://examplebucket.s3.amazonaws.com/test$file.text", "PUT", headers, auth_token, err_msg));
    ASSERT_EQ(err_msg, "");
    ASSERT_EQ(auth_token, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=date;host;x-amz-content-sha256;x-amz-date;x-amz-storage-class,Signature=98ad721746da40c64f1a55b78f14c238d841ea1380cd77a1b5971af0ece108bd");
}

TEST(Factory, PathEncode) {
    ASSERT_EQ(Factory::PathEncode("https://examplebucket.s3.amazonaws.com/test.txt"), "/test.txt");
    ASSERT_EQ(Factory::PathEncode("https://examplebucket.s3.amazonaws.com/test//sub.txt"), "/test//sub.txt");
    ASSERT_EQ(Factory::PathEncode("https://examplebucket.s3.amazonaws.com/test.txt?foo=bar"), "/test.txt");
    ASSERT_EQ(Factory::PathEncode("https://examplebucket.s3.amazonaws.com?lifecycle"), "/");
    ASSERT_EQ(Factory::PathEncode("https://examplebucket.s3.amazonaws.com/test$file.text"), "/test\%24file.text");
}

TEST(Factory, CleanObjectName) {
    ASSERT_EQ(Factory::CleanObjectName("test.txt"), "test.txt");
    ASSERT_EQ(Factory::CleanObjectName("test//sub.txt"), "test//sub.txt");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?foo=bar"), "test.txt?foo=bar");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?authz="), "test.txt");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?authz=foo"), "test.txt");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?&authz=foo"), "test.txt");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?foo=bar&authz=foo"), "test.txt?foo=bar");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?foo=bar&authz=foo&authz=bar"), "test.txt?foo=bar");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?foo=bar&authz=foo&foo=bar"), "test.txt?foo=bar&foo=bar");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?foo=bar&authz=foo&foo=bar"), "test.txt?foo=bar&foo=bar");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?&foo=bar&authz=foo&foo=bar"), "test.txt?foo=bar&foo=bar");
    ASSERT_EQ(Factory::CleanObjectName("test.txt?&foo=bar&oss.asize&authz=foo&foo=bar"), "test.txt?foo=bar&oss.asize&foo=bar");
}

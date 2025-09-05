/***************************************************************
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

#ifndef XRDCLS3_S3FACTORY_HH
#define XRDCLS3_S3FACTORY_HH

#include <XrdCl/XrdClPlugInInterface.hh>

#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>

namespace XrdCl {
    class Log;
}

namespace XrdClS3 {

const uint64_t kLogXrdClS3 = 73174;

class Factory final : public XrdCl::PlugInFactory {
public:
    Factory();
    virtual ~Factory() {}
    Factory(const Factory &) = delete;

    virtual XrdCl::FilePlugIn *CreateFile(const std::string &url) override;
    virtual XrdCl::FileSystemPlugIn *CreateFileSystem(const std::string &url) override;

    // Given a S3-style url (s3://bucket/object), return the corresponding HTTPS URL.
    //
    // If "obj_result" is not nullptr, then it will be set to the object/key and the resulting HTTPS URL
    // will not include the key name.
    static bool GenerateHttpUrl(const std::string &s3_url, std::string &https_url, std::string *obj_result, std::string &err_msg);

    // Convenience function to extract the hostname from a URL.
    static std::string_view ExtractHostname(const std::string_view &url);

    // Convenience function to trim the leading and trailing whitespace from a string.
    static std::string_view TrimView(const std::string_view &str);


    // Generate a V4 signature for the given HTTP URL, returning the
    // authorization token in auth_token.  Returns true on success, false
    // on failure, with an error message in err_msg.
    static bool GenerateV4Signature(const std::string &url, const std::string &verb, std::vector<std::pair<std::string, std::string>> &headers, std::string &auth_token, std::string &err_msg);

    // Given a HTTPS URL representing a S3 object request, return the bucket name.
    //
    // Parses the bucket name from the hostname (for virtual-hosted-style URLs) or
    // from the path (for path-style URLs).
    static std::string GetBucketFromHttpsUrl(const std::string &url);

    // Given a bucket name, return the credentials to use.
    static std::tuple<std::string, std::string, bool> GetCredentialsForBucket(const std::string &bucket, std::string &err_msg);

    // Helper function to convert the URL to a canonical path form for V4 signing.
    static std::string PathEncode(const std::string_view url);

    // Helper function to remove XRootD-specific query parameters from an object name
    static std::string CleanObjectName(const std::string &object);

    static const std::string &GetMkdirSentinel() {return m_mkdir_sentinel;}

    // Setters for the S3 endpoint, service, region, and URL style.
    // Intended to be used for testing or configuration purposes.
    static void SetEndpoint(const std::string &endpoint) { m_endpoint = endpoint; }
    static void SetService(const std::string &service) { m_service = service; }
    static void SetRegion(const std::string &region) { m_region = region; }
    static void SetUrlStyle(const std::string &url_style) { m_url_style = url_style; }
    static void SetDefaultCredentials(const std::string &access_key, const std::string &secret_key) {
        m_default_creds.m_accesskey = access_key;
        m_default_creds.m_secretkey = secret_key;
    }
    static void SetBucketCredentials(const std::string &bucket, const std::string &access_key, const std::string &secret_key) {
        m_bucket_location_map[bucket] = {access_key, secret_key};
    }
    static void ResetCredCache() {
        std::unique_lock lock(m_bucket_auth_map_mutex);
        m_bucket_auth_map.clear();
    }

private:

    // Iterate through the client configuration and determine the S3
    // settings
    static void InitS3Config();

    // Helper function to canonicalize the query string for V4 signing.
    static std::string CanonicalizeQueryString(const std::string &url);

    static bool m_initialized;
    static XrdCl::Log *m_log;
    static std::once_flag m_init_once;

    // Endpoint for the S3 service we will use
    static std::string m_endpoint;
    static std::string m_service;
    static std::string m_region;
    static std::string m_url_style;

    // S3 doesn't have the concept of "directories"; if a given name
    // (some/path) has an object containing it as a prefix
    // (some/path/foo.txt), then we say it's a directory.  Hence, to
    // "make" a directory, we create a zero-length sentinel file inside
    // it; this static variable controls the name.
    static std::string m_mkdir_sentinel;

    // Struct describing S3 credentials
    struct Credentials {
        std::string m_accesskey;
        std::string m_secretkey;
    };

    // Default credentials if not specified for a bucket.
    static Credentials m_default_creds;

    // Map from bucket name to credentials to use
    static std::unordered_map<std::string, Credentials> m_bucket_location_map;

    // Shared mutex to protect access to the bucket credentials map.
    static std::shared_mutex m_bucket_auth_map_mutex;

    // Map for bucket-to-credential info.
    static std::unordered_map<std::string, std::pair<Credentials, std::chrono::steady_clock::time_point>> m_bucket_auth_map;
};

}

#endif // XRDCLS3_S3FACTORY_HH

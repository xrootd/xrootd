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

#include "XrdClS3Factory.hh"
#include "XrdClS3File.hh"
#include "XrdClS3Filesystem.hh"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>

#include <fcntl.h>

XrdVERSIONINFO(XrdClGetPlugIn, XrdClGetPlugIn)

using namespace XrdClS3;

std::shared_mutex Factory::m_bucket_auth_map_mutex;
bool Factory::m_initialized = false;
XrdCl::Log *Factory::m_log{nullptr};
std::once_flag Factory::m_init_once;
std::string Factory::m_endpoint = "";
std::string Factory::m_service = "s3";
std::string Factory::m_region = "us-east-1";
std::string Factory::m_url_style = "virtual";
std::string Factory::m_mkdir_sentinel;
Factory::Credentials Factory::m_default_creds;
std::unordered_map<std::string, Factory::Credentials> Factory::m_bucket_location_map;
std::unordered_map<std::string, std::pair<Factory::Credentials, std::chrono::steady_clock::time_point>> Factory::m_bucket_auth_map;


namespace {

std::string
AmazonURLEncode(const std::string &input) {
	/*
	 * See
	 * http://docs.amazonwebservices.com/AWSEC2/2010-11-15/DeveloperGuide/using-query-api.html
	 *
	 */
	std::string output;
    output.reserve(input.size());
	for (const auto & val : input) {
		// "Do not URL encode ... A-Z, a-z, 0-9, hyphen ( - ),
		// underscore ( _ ), period ( . ), and tilde ( ~ ).  Percent
		// encode all other characters with %XY, where X and Y are hex
		// characters 0-9 and uppercase A-F.  Percent encode extended
		// UTF-8 characters in the form %XY%ZA..."
		if (('A' <= val && val <= 'Z') ||
			('a' <= val && val <= 'z') ||
			('0' <= val && val <= '9') || val == '-' ||
			val == '_' || val == '.' || val == '~') {
			output.append(1, val);
		} else {
			char percentEncode[4];
			snprintf(percentEncode, 4, "%%%.2hhX", val);
			output.append(percentEncode);
		}
	}
	return output;
}

}

Factory::Factory() {
    std::call_once(m_init_once, [&] {
        m_log = XrdCl::DefaultEnv::GetLog();
        if (!m_log) {
            return;
        }
        m_log->SetTopicName(kLogXrdClS3, "XrdClS3");

        auto env = XrdCl::DefaultEnv::GetEnv();
        if (!env) {
            return;
        }
        InitS3Config();
        m_initialized = true;
    });
}

std::string
Factory::CanonicalizeQueryString(const std::string &url) {
    auto loc = url.find("://");
    if (loc == std::string::npos) {
        return "";
    }
    loc += 3; // Skip the "://"
    loc = url.find('?', loc);
    if (loc == std::string::npos) {
        return "";
    }
    std::vector<std::pair<std::string, std::string>> query_parameters;
    auto param_end = url.find('&', loc);
    while (loc != std::string::npos) {
        auto param_start = loc + 1; // Skip the '?' / '&'
        loc = url.find('=', param_start);
        if (loc == param_start) {
            // Empty parameter name, skip
        }
        else if (loc >= param_end) {
            auto param = url.substr(param_start, param_end - param_start);
            if (!param.empty()) {
                // No '=' found, treat as a parameter without value
                query_parameters.emplace_back(AmazonURLEncode(param), "");
            }
        } else {
            std::string name = url.substr(param_start, loc - param_start);
            loc++; // Move past '='
            auto value_start = loc;
            std::string value;
            if (param_end == std::string::npos) {
                value = url.substr(value_start);
            } else {
                value = url.substr(value_start, param_end - value_start);
            }
            if (!value.empty()) {
                query_parameters.emplace_back(AmazonURLEncode(name), AmazonURLEncode(value));
            }
        }
        loc = param_end;
        if (loc != std::string::npos) {
            param_end = url.find('&', loc + 1);
        }
    }
    std::sort(query_parameters.begin(), query_parameters.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    size_t string_size = 0;
    for (const auto &param : query_parameters) {
        string_size += param.first.size() + param.second.size() + 2;
    }
	std::string canonicalQueryString;
    if (string_size) {
        canonicalQueryString.reserve(string_size);
    }
	for (const auto &param : query_parameters) {

		// Step 1C: Separate parameter names from values with '='.
		canonicalQueryString += param.first + '=' + param.second;

		// Step 1D: Separate name-value pairs with '&';
		canonicalQueryString += '&';
	}
	// We'll always have a superflous trailing ampersand.
	if (!canonicalQueryString.empty()) {
		canonicalQueryString.erase(canonicalQueryString.end() - 1);
	}
	return canonicalQueryString;
}

XrdCl::FilePlugIn *
Factory::CreateFile(const std::string & /*url*/) {
    if (!m_initialized) {return nullptr;}
    return new File(m_log);
}

XrdCl::FileSystemPlugIn *
Factory::CreateFileSystem(const std::string & url) {
    if (!m_initialized) {return nullptr;}
    return new Filesystem(url, m_log);
}

namespace {

void SetDefault(XrdCl::Env *env, const std::string &optName, const std::string &envName, std::string &location, const std::string &def) {
    std::string val;
    if (!env->GetString(optName, val) || val.empty()) {
        env->PutString(optName, "");
        env->ImportString(optName, envName);
    }
    if (env->GetString(optName, val) && !val.empty()) {
        location = val;
    } else {
        location = def;
    }
}

// Trim the left side of a string_view for space
std::string_view ltrim_view(const std::string_view input_view) {
    for (size_t idx = 0; idx < input_view.size(); idx++) {
        if (!isspace(input_view[idx])) {
            return input_view.substr(idx);
        }
    }
    return "";
}

bool ComputeSHA256(const std::string_view payload, std::vector<unsigned char> &messageDigest) {
	EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
	if (mdctx == NULL) {
		return false;
	}

	if (!EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
		EVP_MD_CTX_destroy(mdctx);
		return false;
	}

	if (!EVP_DigestUpdate(mdctx, payload.data(), payload.length())) {
		EVP_MD_CTX_destroy(mdctx);
		return false;
	}

    unsigned int mdLength;
	if (!EVP_DigestFinal_ex(mdctx, messageDigest.data(), &mdLength)) {
		EVP_MD_CTX_destroy(mdctx);
		return false;
	}
    messageDigest.resize(mdLength);

	EVP_MD_CTX_destroy(mdctx);
	return true;
}

void MessageDigestAsHex(const std::vector<unsigned char> messageDigest,
										std::string &hexEncoded) {
    hexEncoded.resize(messageDigest.size() * 2);
	char *ptr = hexEncoded.data();
	for (unsigned int idx = 0; idx < messageDigest.size(); ++idx, ptr += 2) {
		snprintf(ptr, 3, "%02x", messageDigest[idx]);
	}
}

// Helper function to read a file descriptor until EOF or
// `nbytes` bytes have been read.
// Includes appropriate handling of EINTR.
ssize_t FullRead(int fd, void *ptr, size_t nbytes) {
	ssize_t nleft, nread;

	nleft = nbytes;
	while (nleft > 0) {
	REISSUE_READ:
		nread = read(fd, ptr, nleft);
		if (nread < 0) {
			if (errno == EINTR) {
				goto REISSUE_READ;
			}
			return -1;
		} else if (nread == 0) {
			break;
		}
		nleft -= nread;
		ptr = static_cast<char *>(ptr) + nread;
	}
	return (nbytes - nleft);
}

// Read a file into a string.
// If the file is larger than 32k, it will return false.
bool
ReadShortFile(const std::string &fileName, std::string &contents, std::string &err_msg) {
	int fd = open(fileName.c_str(), O_RDONLY, 0600);
	if (fd < 0) {
        err_msg = "Failed to open file '" + fileName + "': " + std::string(strerror(errno));
		return false;
	}
    contents.resize(32*1024);

    auto totalRead = FullRead(fd, contents.data(), contents.size());
	close(fd);
	if (totalRead == -1) {
        err_msg = "Failed to read file '" + fileName + "': " + std::string(strerror(errno));
		return false;
	}
    contents.resize(totalRead);
	return true;
}

} // namespace

std::string
Factory::CleanObjectName(const std::string & input_obj) {
    std::string obj = input_obj;
    auto loc = input_obj.find('?');
    if (loc != std::string::npos) {
        auto query = std::string_view(input_obj).substr(loc + 1);
        obj = obj.substr(0, loc);
        bool added_query = false;
        while (!query.empty()) {
            auto next_query_loc = query.find('&');
            auto current_query = (next_query_loc == std::string::npos) ? query : query.substr(0, next_query_loc);
            query = (next_query_loc == std::string::npos) ? "" : query.substr(next_query_loc + 1);
            if (current_query.empty()) {
                continue;
            }
            auto equal_loc = current_query.find('=');
            if (equal_loc != std::string::npos) {
                auto key = current_query.substr(0, equal_loc);
                if (key != "authz") {
                    obj += (added_query ? "&" : "?") + std::string(current_query);
                    added_query = true;
                }
            } else if (current_query != "authz") {
                obj += (added_query ? "&" : "?") + std::string(current_query);
                added_query = true;
            }
        }
    } else {
        obj = input_obj;
    }
    return obj;
}

std::string_view
Factory::ExtractHostname(const std::string_view url) {
    auto loc = url.find("://");
    if (loc == std::string_view::npos) {
        return {};
    }
    loc += 3; // Move past "://"
    auto slash_loc = url.find('/', loc);
    auto query_loc = url.find('?', loc);
    if (query_loc != std::string_view::npos && (slash_loc == std::string_view::npos || query_loc < slash_loc)) {
        slash_loc = query_loc; // If there's a query, we stop at it
    }
    auto authority = url.substr(loc, slash_loc - loc);
    if (authority.empty()) {
        return {};
    }
    auto at_loc = authority.find('@');
    if (at_loc != std::string_view::npos) {
        // If there's an '@', we have user info, so we skip it
        authority = authority.substr(at_loc + 1);
    }
    // If the authority contains a port, we need to strip it
    auto colon_loc = authority.find(':');
    if (colon_loc != std::string_view::npos) {
        authority = authority.substr(0, colon_loc);
    }
    return authority;
}

void
Factory::InitS3Config()
{
    auto env = XrdCl::DefaultEnv::GetEnv();
    SetDefault(env, "XrdClS3MkdirSentinel", "XRDCLS3_MKDIRSENTINEL", m_mkdir_sentinel, ".xrdcls3.dirsentinel");
    SetDefault(env, "XrdClS3Endpoint", "XRDCLS3_ENDPOINT", m_endpoint, "");
    SetDefault(env, "XrdClS3UrlStyle", "XRDCLS3_URLSTYLE", m_url_style, "virtual");
    SetDefault(env, "XrdClS3Region", "XRDCLS3_REGION", m_region, "us-east-1");
    std::string access_key;
    SetDefault(env, "XrdClS3AccessKeyLocation", "XRDCLS3_ACCESSKEYLOCATION", access_key, "");
    std::string secret_key;
    SetDefault(env, "XrdClS3SecretKeyLocation", "XRDCLS3_SECRETKEYLOCATION", secret_key, "");
    if (!access_key.empty() && !secret_key.empty()) {
        m_default_creds = {access_key, secret_key};
    } else if (access_key.empty() && secret_key.empty()) {
        m_log->Info(kLogXrdClS3, "Defaulting to public bucket access");
    } else if (access_key.empty() && !secret_key.empty()) {
        m_log->Warning(kLogXrdClS3, "Secret key location set (%s) but access key location is empty; authorization will not work.", secret_key.c_str());
    } else if (!access_key.empty() && secret_key.empty()) {
        m_log->Warning(kLogXrdClS3, "Access key location set (%s) but secret key location is empty; authorization will not work.", access_key.c_str());
    }

    // Parse the per-bucket configuration of credentials.
    std::string bucket_configs;
    SetDefault(env, "XrdClS3BucketConfigs", "XRDCLS3_BUCKETCONFIGS", bucket_configs, "");
    if (!bucket_configs.empty()) {
        std::stringstream ss(bucket_configs);
        std::string config_name;
        while (std::getline(ss, config_name)) {
            auto name = TrimView(config_name);
            auto bucket_name_key = std::string("XrdClS3") + std::string(name) + "BucketName";
            std::string bucket_name_val;
            if (!env->GetString(bucket_name_key, bucket_name_val) || bucket_name_val.empty()) {
                m_log->Warning(kLogXrdClS3, "Per-bucket config includes entry '%s' but XrdClS3%sBucketName is not set", std::string(name).c_str(), std::string(name).c_str());
                continue;
            }
            auto access_key_location_key = std::string("XrdClS3") + std::string(name) + "AccessKeyLocation";
            std::string access_key_location_val;
            auto has_access_key = env->GetString(access_key_location_key, access_key_location_val) && !access_key_location_val.empty();

            auto secret_key_location_key = std::string("XrdClS3") + std::string(name) + "SecretKeyLocation";
            std::string secret_key_location_val;
            auto has_secret_key = env->GetString(secret_key_location_key, secret_key_location_val) && !secret_key_location_val.empty();

            if (has_access_key && has_secret_key) {
                m_bucket_location_map[bucket_name_val] = {access_key_location_val, secret_key_location_val};
            } else if (!has_access_key && !has_secret_key) {
                // If both are empty, then it is implicitly a public bucket.
                m_bucket_location_map[bucket_name_val] = {"", ""};
            } else if (has_access_key && !has_secret_key) {
                m_log->Warning(kLogXrdClS3, "Per-bucket config for entry '%s' has an access key location set (%s) but no secret key", std::string(name).c_str(), access_key_location_val.c_str());
            } else {
                m_log->Warning(kLogXrdClS3, "Per-bucket config for entry '%s' has an secret key location set (%s) but no access key", std::string(name).c_str(), secret_key_location_val.c_str());
            }
        }
    }
}

bool
Factory::GenerateHttpUrl(const std::string &s3_url, std::string &https_url, std::string *obj_result, std::string &err_msg) {
    if (s3_url.substr(0, 5) != "s3://") {
        err_msg = "Provided URL does not start with s3://";
        return false;
    }
    auto loc = s3_url.find('/', 5);
    auto bucket = s3_url.substr(5, loc - 5);
    auto at_loc = bucket.find('@');
    if (at_loc != std::string::npos) {
        std::string login = "";
        login = bucket.substr(0, at_loc);
        bucket = bucket.substr(at_loc + 1);
    }
    std::string endpoint = m_endpoint;
    std::string region = m_region;
    if ((bucket == m_endpoint) || m_endpoint.empty()) {
        endpoint = bucket;
        auto old_loc = loc + 1;
        loc = s3_url.find('/', loc + 1);
        if (loc == std::string::npos) {
            err_msg = "Provided S3 URL does not contain a bucket in path";
            return false;
        }
        bucket = s3_url.substr(old_loc, loc - old_loc);
    } else {
        auto authority = ExtractHostname(s3_url);
        std::string test_endpoint = "." + endpoint;
        if (!m_region.empty()) {
            auto bucket_loc = authority.rfind("." + m_region + test_endpoint);
            if (bucket_loc != std::string::npos) {
                bucket = authority.substr(0, bucket_loc);
            } else {
                auto bucket_loc = authority.rfind(test_endpoint);
                if (bucket_loc != std::string::npos) {
                    bucket = authority.substr(0, bucket_loc);
                }
            }
        } else {
            auto bucket_loc = authority.rfind(test_endpoint);
            if (bucket_loc != std::string::npos) {
                bucket = authority.substr(0, bucket_loc);
            }
        }
    }
    std::string obj;
    if (loc != std::string::npos) {
        obj = s3_url.substr(loc + 1);
    }
    // Strip out "authz" query parameters; those are internal to XRootD.
    obj = CleanObjectName(obj);
    if (obj_result) {
        *obj_result = obj;
    }
    if (m_url_style == "virtual" || m_url_style.empty()) {
        https_url = "https://" + bucket + "." + m_region + "." + endpoint + (obj_result ? "" : ("/" + obj));
        return true;
    } else if (m_url_style == "path") {
        if (m_region.empty()) {
            https_url = "https://" + m_region + "." + endpoint + "/" + bucket + (obj_result ? "" : ("/" + obj));
        } else {
            https_url = "https://" + endpoint + "/" + bucket + (obj_result ? "" : ("/" + obj));
        }
        return true;
    } else {
        err_msg = "Server configuration has invalid setting for URL style";
        return false;
    }
}

bool
Factory::GenerateV4Signature(const std::string &url, const std::string &verb, std::vector<std::pair<std::string, std::string>> &headers, std::string &auth_token, std::string &err_msg) {
    auto bucket = GetBucketFromHttpsUrl(url);

    // If we're using temporary credentials, we need to add the token
    // header here as well.  We set saKey and keyID here (well before
    // necessary) since we'll get them for free when we get the token.
    auto [keyId, secretKey, ok] = GetCredentialsForBucket(bucket, err_msg);
    if (!ok) {
        return false;
    }

    if (secretKey.empty()) {
        auth_token = "";
        return true;
    }

    //
    // Create task 1's inputs.
    //

    auto canonicalURI = PathEncode(url);

    // The canonical query string is the alphabetically sorted list of
    // URI-encoded parameter names '=' values, separated by '&'s.
    auto canonicalQueryString = CanonicalizeQueryString(url);

    // The canonical headers must include the Host header, so add that
    // now if we don't have it.
    if (std::find_if(headers.begin(), headers.end(), 
                        [](const auto &pair) { return pair.first == "Host"; }) == headers.end()) {
        auto host = ExtractHostname(url);
        if (host.empty()) {
            err_msg = "Unable to extract hostname from URL: " + url;
            return false;
        }
        headers.emplace_back("Host", host);
    }

    // S3 complains if x-amz-date isn't signed, so do this early.
    auto iter = std::find_if(headers.begin(), headers.end(),
        [](const auto &pair) { return !strcasecmp(pair.first.c_str(), "X-Amz-Date"); });
    std::string date_time;
    char date_char[] = "YYYYMMDD";
    if (iter == headers.end()) {
        time_t now;
        time(&now);
        struct tm brokenDownTime;
        gmtime_r(&now, &brokenDownTime);

        date_time =  "YYYYMMDDThhmmssZ";
        strftime(date_time.data(), date_time.size(), "%Y%m%dT%H%M%SZ", &brokenDownTime);
        headers.emplace_back("X-Amz-Date", date_time);
        strftime(date_char, sizeof(date_char), "%Y%m%d", &brokenDownTime);
    } else {
        date_time = iter->second;
        auto loc = date_time.find('T', 0);
        if (loc != 8) {
            err_msg = "Invalid value for X-Amz-Date";
            return false;
        }
        memcpy(date_char, date_time.c_str(), 8);
    }

    // The canonical payload hash is the lowercase hexadecimal string of the
    // (SHA256) hash value of the payload or "UNSIGNED-PAYLOAD" if
    // we are not signing the payload.
    std::string payload_hash = "UNSIGNED-PAYLOAD";
    iter = std::find_if(headers.begin(), headers.end(),
        [](const auto &pair) { return !strcasecmp(pair.first.c_str(), "X-Amz-Content-Sha256"); });
    if (iter == headers.end()) {
        headers.emplace_back("X-Amz-Content-Sha256", payload_hash);
    } else {
        payload_hash = iter->second;
    }

    // The canonical list of headers is a sorted list of lowercase header
    // names paired via ':' with the trimmed header value, each pair
    // terminated with a newline.
    std::vector<std::pair<std::string, std::string>> transformed_headers;
    transformed_headers.reserve(headers.size());
    for (const auto &info : headers) {
        std::string header = info.first;
        std::transform(header.begin(), header.end(), header.begin(), &tolower);

        std::string value = info.second;
        if (value.empty()) {
            continue;
        }
        auto value_trimmed = std::string(TrimView(value));

        // Convert internal runs of spaces into single spaces.
        unsigned left = 1;
        unsigned right = 1;
        bool inSpaces = false;
        while (right < value_trimmed.length()) {
            if (!inSpaces) {
                if (value_trimmed[right] == ' ') {
                    inSpaces = true;
                    left = right;
                    ++right;
                } else {
                    ++right;
                }
            } else {
                if (value_trimmed[right] == ' ') {
                    ++right;
                } else {
                    inSpaces = false;
                    value_trimmed.erase(left, right - left - 1);
                    right = left + 1;
                }
            }
        }

        transformed_headers.emplace_back(header, value);
    }
    std::sort(transformed_headers.begin(), transformed_headers.end(),
                [](const auto &a, const auto &b) { return a.first < b.first; });

    // The canonical list of signed headers is trivial to generate while
    // generating the list of headers.
    std::string signedHeaders, canonicalHeaders;
    for (const auto &info : transformed_headers) {
        canonicalHeaders += info.first + ":" + info.second + "\n";
        signedHeaders += info.first + ";";
    }
    signedHeaders.erase(signedHeaders.end() - 1);

    // Task 1: create the canonical request.
    auto canonicalRequest =
        verb + "\n" + canonicalURI + "\n" + canonicalQueryString + "\n" +
        canonicalHeaders + "\n" + signedHeaders + "\n" + payload_hash;

    //
    // Create task 2's inputs.
    //

    // Hash the canonical request the way we did the payload.
    std::string canonicalRequestHash;
    std::vector<unsigned char> messageDigest;
    messageDigest.resize(EVP_MAX_MD_SIZE);
    if (!ComputeSHA256(canonicalRequest, messageDigest)) {
        err_msg = "Unable to hash canonical request.";
        return false;
    }
    MessageDigestAsHex(messageDigest, canonicalRequestHash);

    // Task 2: create the string to sign.
    auto credentialScope = std::string(date_char) + "/" + m_region + "/" + m_service + "/aws4_request";
    auto stringToSign = std::string("AWS4-HMAC-SHA256\n") + date_time + "\n" + credentialScope + "\n" + canonicalRequestHash;

    //
    // Creating task 3's inputs was done when we checked to see if we needed
    // to get the security token, since they come along for free when we do.
    //

    // Task 3: calculate the signature.
    auto saKey = std::string("AWS4") + secretKey;
    unsigned int mdLength = 0;
    const unsigned char *hmac =
        HMAC(EVP_sha256(), saKey.c_str(), saKey.length(), (unsigned char *)date_char,
                sizeof(date_char) - 1, messageDigest.data(), &mdLength);
    if (hmac == NULL) {
        err_msg = "Unable to calculate HMAC for date.";
        return false;
    }

    unsigned int md2Length = 0;
    unsigned char messageDigest2[EVP_MAX_MD_SIZE];
    hmac = HMAC(EVP_sha256(), messageDigest.data(), mdLength,
                reinterpret_cast<unsigned char *>(m_region.data()), m_region.size(), messageDigest2,
                &md2Length);
    if (hmac == NULL) {
        err_msg = "Unable to calculate HMAC for region.";
        return false;
    }

    hmac = HMAC(EVP_sha256(), messageDigest2, md2Length,
                reinterpret_cast<unsigned char *>(m_service.data()), m_service.size(), messageDigest.data(),
                &mdLength);
    if (hmac == NULL) {
        err_msg = "Unable to calculate HMAC for service.";
        return false;
    }

    const char request_char[] = "aws4_request";
    hmac = HMAC(EVP_sha256(), messageDigest.data(), messageDigest.size(), reinterpret_cast<const unsigned char *>(request_char),
                sizeof(request_char) - 1, messageDigest2, &md2Length);
    if (hmac == NULL) {
        err_msg = "Unable to calculate HMAC for request.";
        return false;
    }

    hmac = HMAC(EVP_sha256(), messageDigest2, md2Length,
                reinterpret_cast<unsigned char *>(stringToSign.data()),
                stringToSign.size(), messageDigest.data(), &mdLength);
    if (hmac == NULL) {
        err_msg = "Unable to calculate HMAC for request string.";
        return false;
    }

    std::string signature;
    MessageDigestAsHex(messageDigest, signature);

    auth_token =
                std::string("AWS4-HMAC-SHA256 Credential=") + keyId + "/" + credentialScope +
                ",SignedHeaders=" + signedHeaders + ",Signature=" + signature;
    return true;
}

std::string
Factory::GetBucketFromHttpsUrl(const std::string &url) {
    if (m_url_style == "virtual" || m_url_style.empty()) {
        // Virtual-hosted-style URLs are of the form https://bucket.region.endpoint/object
        auto hostname = ExtractHostname(url);
        if (hostname.empty()) {
            return {};
        }
        auto test_endpoint = "." + m_endpoint;
        if (!m_region.empty()) test_endpoint = "." + m_region + test_endpoint;
        auto loc = hostname.rfind(test_endpoint);
        if (loc == std::string::npos) {
            if (!m_region.empty()) {
                loc = hostname.rfind("." + m_endpoint);
                if (loc != std::string::npos) {
                    return std::string(hostname.substr(0, loc));
                }
            }
            return {};
        }
        return std::string(hostname.substr(0, loc));
    } else if (m_url_style == "path") {
        // Path style URLs are of the form https://region.endpoint/bucket/object
        auto loc = url.find("://");
        if (loc == std::string::npos) {
            return {};
        }
        loc += 3; // Move past "://"
        auto slash_loc = url.find('/', loc);
        if (slash_loc == std::string::npos) {
            return {};
        }
        auto bucket_start = slash_loc + 1;
        auto bucket_end = url.find('/', bucket_start);
        if (bucket_end == std::string::npos) {
            return url.substr(bucket_start);
        }
        return url.substr(bucket_start, bucket_end - bucket_start);
    } else {
        // Invalid URL style
        return {};
    }
}

std::tuple<std::string, std::string, bool>
Factory::GetCredentialsForBucket(const std::string &bucket, std::string &err_msg)
{
    auto now = std::chrono::steady_clock::now();
    {
        std::shared_lock lock(m_bucket_auth_map_mutex);
        auto iter = m_bucket_auth_map.find(bucket);
        if (iter != m_bucket_auth_map.end()) {
            // If we have credentials for this bucket, check if they are still valid.
            auto &creds = iter->second.first;
            auto &expiration = iter->second.second;
            if (now < expiration) {
                // Credentials are still valid, return them.
                return {creds.m_accesskey, creds.m_secretkey, true};
            }
        }
    }

    std::unique_lock lock(m_bucket_auth_map_mutex);
    auto iter = m_bucket_location_map.find(bucket);
    std::string access_key_location, secret_key_location;
    if (iter == m_bucket_location_map.end()) {
        // If we don't have credentials for this bucket, use the default.
        if (m_default_creds.m_accesskey.empty() || m_default_creds.m_secretkey.empty()) {
            // No credentials at all, so we assume public access.
            m_bucket_auth_map[bucket] = {{"", ""}, now + std::chrono::minutes(1)};
            return {"", "", true};
        }
        access_key_location = m_default_creds.m_accesskey;
        secret_key_location = m_default_creds.m_secretkey;
    } else {
        access_key_location = iter->second.m_accesskey;
        secret_key_location = iter->second.m_secretkey;
    }
    if (access_key_location.empty() && secret_key_location.empty()) {
        // If both are empty, we assume public access.
        m_bucket_auth_map[bucket] = {{"", ""}, now + std::chrono::minutes(1)};
        return {"", "", true};
    }
    if (access_key_location.empty() || secret_key_location.empty()) {
        err_msg = "No credentials available for bucket: " + bucket;
        m_bucket_auth_map[bucket] = {{"", ""}, now + std::chrono::seconds(10)};
        return {"", "", false};
    }

    std::string access_key, secret_key;
    if (!ReadShortFile(access_key_location, access_key, err_msg)) {
        m_bucket_auth_map[bucket] = {{"", ""}, now + std::chrono::seconds(10)};
        return {"", "", false};
    }
    access_key = TrimView(access_key);

    if (!ReadShortFile(secret_key_location, secret_key, err_msg)) {
        m_bucket_auth_map[bucket] = {{"", ""}, now + std::chrono::seconds(10)};
        return {"", "", false};
    }
    secret_key = TrimView(secret_key);

    if (access_key.empty() || secret_key.empty()) {
        err_msg = "Credentials for bucket '" + bucket + "' are empty.";
        m_bucket_auth_map[bucket] = {{"", ""}, now + std::chrono::seconds(10)};
        return {"", "", false};
    }
    m_bucket_auth_map[bucket] = {{access_key, secret_key}, now + std::chrono::minutes(1)};
    return {access_key, secret_key, true};
}

std::string
Factory::PathEncode(const std::string_view url) {
    auto loc = url.find("://");
    if (loc == std::string_view::npos) {
        return "";
    }
    auto path_loc = url.find("/", loc + 3);
    auto query_loc = url.find("?", loc + 3);
    if (query_loc != std::string_view::npos && (path_loc == std::string_view::npos || query_loc < path_loc)) {
        // No path, just a query string
        return "/";
    }
    auto path = url.substr(path_loc, query_loc - path_loc);
    std::string segment;
	std::string encoded;
	
	size_t next = 0;
	size_t offset = 0;
	const auto length = path.size();
	while (offset < length) {
		next = strcspn(path.data() + offset, "/");
		if (next == 0) {
			encoded += "/";
			offset += 1;
			continue;
		}
        if (offset + next >= length) {
            next = length - offset;
        }

		segment = std::string(path.data() + offset, next);
		encoded += AmazonURLEncode(segment);

		offset += next;
	}
	return encoded;
}

// Trim left and right side of a string_view for space characters
std::string_view
Factory::TrimView(const std::string_view input_view) {
    auto view = ltrim_view(input_view);
    for (size_t idx = 0; idx < input_view.size(); idx++) {
        if (!isspace(view[view.size() - 1 - idx])) {
            return view.substr(0, view.size() - idx);
        }
    }
    return "";
}

extern "C"
{
    void *XrdClGetPlugIn(const void*)
    {
        return static_cast<void*>(new Factory());
    }
}

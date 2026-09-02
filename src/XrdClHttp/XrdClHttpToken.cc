/******************************************************************************/
/* Copyright (C) 2026 by European Organization for Nuclear Research (CERN)   */
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
/******************************************************************************/

#include "XrdClHttpToken.hh"

#include <XrdOuc/XrdOucJson.hh>

#include <limits>

namespace {

const std::vector<std::string> &DefaultActivities(bool write)
{
    static const std::vector<std::string> read{"LIST", "DOWNLOAD"};
    static const std::vector<std::string> write_access{
        "LIST", "DOWNLOAD", "MANAGE", "UPLOAD", "DELETE"
    };
    return write ? write_access : read;
}

bool ValidPort(std::string_view port)
{
    if (port.empty()) return false;
    unsigned int value = 0;
    for (char character : port) {
        if (character < '0' || character > '9') return false;
        value = value * 10 + static_cast<unsigned int>(character - '0');
        if (value > 65535) return false;
    }
    return true;
}

bool ParseIssuerUrl(std::string_view issuer, std::string &authority,
                    std::string &path)
{
    authority.clear();
    path.clear();

    std::string normalized;
    if (!XrdClHttp::NormalizeTokenUrl(issuer, normalized)) return false;

    constexpr std::string_view prefix{"https://"};
    auto authority_end = normalized.find_first_of("/?#", prefix.size());
    if (authority_end == std::string::npos) authority_end = normalized.size();
    auto authority_view = std::string_view(normalized).substr(
        prefix.size(), authority_end - prefix.size());

    // Never forward embedded credentials to a discovery endpoint. Query and
    // fragment components are not part of either discovery URL and are
    // intentionally omitted, matching gfal2's URL transformation.
    if (authority_view.find('@') != std::string_view::npos) return false;

    authority.assign(prefix);
    authority.append(authority_view);
    if (authority_end < normalized.size() &&
        normalized[authority_end] == '/') {
        auto path_end = normalized.find_first_of("?#", authority_end);
        path = normalized.substr(authority_end, path_end - authority_end);
    }
    return true;
}

std::string PercentEncode(std::string_view input)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(input.size());
    for (unsigned char byte : input) {
        if ((byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' ||
            byte == '_' || byte == '~') {
            encoded += static_cast<char>(byte);
        } else {
            encoded += '%';
            encoded += hex[byte >> 4];
            encoded += hex[byte & 0x0f];
        }
    }
    return encoded;
}

} // namespace

bool
XrdClHttp::NormalizeTokenUrl(std::string_view input, std::string &https_url)
{
    https_url.clear();
    auto separator = input.find("://");
    if (separator == std::string_view::npos) {
        return false;
    }
    auto scheme = input.substr(0, separator);
    std::string lowered_scheme;
    lowered_scheme.reserve(scheme.size());
    for (char value : scheme) {
        lowered_scheme += value >= 'A' && value <= 'Z'
            ? static_cast<char>(value - 'A' + 'a') : value;
    }
    if (lowered_scheme != "https" && lowered_scheme != "davs") return false;

    auto authority_start = separator + 3;
    https_url = "https://";
    https_url.append(input.substr(authority_start));

    auto authority_end = input.find_first_of("/?#", authority_start);
    if (authority_end == std::string_view::npos) authority_end = input.size();
    auto authority = input.substr(authority_start,
                                  authority_end - authority_start);
    auto userinfo_end = authority.rfind('@');
    auto host_port = userinfo_end == std::string_view::npos
        ? authority : authority.substr(userinfo_end + 1);
    if (host_port.empty()) {
        https_url.clear();
        return false;
    }

    std::string_view host;
    if (host_port.front() == '[') {
        auto bracket = host_port.find(']');
        if (bracket == std::string_view::npos) {
            https_url.clear();
            return false;
        }
        host = host_port.substr(1, bracket - 1);
        auto remainder = host_port.substr(bracket + 1);
        if (!remainder.empty() &&
            (remainder.front() != ':' || !ValidPort(remainder.substr(1)))) {
            https_url.clear();
            return false;
        }
    } else {
        auto colon = host_port.rfind(':');
        if (colon != std::string_view::npos &&
            host_port.find(':') != colon) {
            https_url.clear();
            return false;
        }
        host = colon == std::string_view::npos
            ? host_port : host_port.substr(0, colon);
        if (colon != std::string_view::npos &&
            !ValidPort(host_port.substr(colon + 1))) {
            https_url.clear();
            return false;
        }
    }
    if (host.empty()) {
        https_url.clear();
        return false;
    }

    for (char value : input) {
        auto byte = static_cast<unsigned char>(value);
        if (byte <= 0x20 || byte == 0x7f) {
            https_url.clear();
            return false;
        }
    }
    return true;
}

bool
XrdClHttp::ParseTokenRequest(std::string_view input, TokenRequest &request,
                             std::string &error)
{
    error.clear();
    request = TokenRequest{};

    auto parsed = nlohmann::json::parse(input.begin(), input.end(), nullptr,
                                        false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        error = "Token query must be a valid JSON object";
        return false;
    }

    auto path = parsed.find("path");
    if (path == parsed.end() || !path->is_string() ||
        path->get_ref<const std::string &>().empty() ||
        path->get_ref<const std::string &>().front() != '/') {
        error = "Token query requires a non-empty absolute string 'path'";
        return false;
    }
    request.path = path->get<std::string>();

    auto issuer = parsed.find("issuer");
    if (issuer != parsed.end()) {
        if (!issuer->is_string() ||
            issuer->get_ref<const std::string &>().empty()) {
            error = "Token query 'issuer' must be a non-empty string";
            return false;
        }
        request.issuer = issuer->get<std::string>();
    }

    auto validity = parsed.find("validity");
    if (validity != parsed.end()) {
        if (!validity->is_number_integer()) {
            error = "Token query 'validity' must be a nonnegative integer";
            return false;
        }
        if (validity->is_number_unsigned()) {
            request.validity = validity->get<std::uint64_t>();
        } else {
            auto signed_validity = validity->get<std::int64_t>();
            if (signed_validity < 0) {
                error = "Token query 'validity' must be a nonnegative integer";
                return false;
            }
            request.validity = static_cast<std::uint64_t>(signed_validity);
        }
    }

    auto write = parsed.find("write");
    if (write != parsed.end()) {
        if (!write->is_boolean()) {
            error = "Token query 'write' must be a boolean";
            return false;
        }
        request.write = write->get<bool>();
    }

    auto activities = parsed.find("activities");
    if (activities != parsed.end()) {
        if (!activities->is_array()) {
            error = "Token query 'activities' must be an array of strings";
            return false;
        }
        for (const auto &activity : *activities) {
            if (!activity.is_string() ||
                activity.get_ref<const std::string &>().empty()) {
                error = "Token query 'activities' must contain non-empty strings";
                return false;
            }
            request.activities.emplace_back(activity.get<std::string>());
        }
    }

    if (request.activities.empty()) {
        request.activities = DefaultActivities(request.write);
    }
    return true;
}

bool
XrdClHttp::BuildOAuthAuthorizationServerUrl(std::string_view issuer,
                                            std::string &url)
{
    url.clear();
    std::string authority;
    std::string path;
    if (!ParseIssuerUrl(issuer, authority, path)) return false;

    url = authority + "/.well-known/oauth-authorization-server";
    if (!path.empty() && path != "/") url += path;
    return true;
}

bool
XrdClHttp::BuildOpenIdConfigurationUrl(std::string_view issuer,
                                       std::string &url)
{
    url.clear();
    std::string authority;
    std::string path;
    if (!ParseIssuerUrl(issuer, authority, path)) return false;

    if (path.empty()) path = "/";
    if (path.back() != '/') path += '/';
    url = authority + path + ".well-known/openid-configuration";
    return true;
}

std::string
XrdClHttp::BuildMacaroonRequest(
    std::uint64_t validity, const std::vector<std::string> &activities)
{
    std::string caveat = "activity:";
    bool first = true;
    for (const auto &activity : activities) {
        if (!first) caveat += ',';
        caveat += activity;
        first = false;
    }

    // Use the JSON serializer for the user-provided caveat while retaining the
    // exact whitespace and member order used by gfal2.
    return "{\"caveats\": [" + nlohmann::json(caveat).dump() +
        "], \"validity\": \"PT" + std::to_string(validity) + "M\"}";
}

std::string
XrdClHttp::BuildSciTokensRequest()
{
    return "grant_type=client_credentials";
}

bool
XrdClHttp::BuildOAuthMacaroonRequest(
    std::string_view path, std::uint64_t validity,
    const std::vector<std::string> &activities, std::string &body,
    std::string &error)
{
    body.clear();
    error.clear();

    auto path_end = path.find_first_of("?#");
    auto scope_path = path.substr(0, path_end);
    if (scope_path.empty() || scope_path.front() != '/') {
        error = "OAuth macaroon scope requires an absolute path";
        return false;
    }
    if (validity > std::numeric_limits<std::uint64_t>::max() / 60) {
        error = "Token validity is too large to convert to seconds";
        return false;
    }

    std::string scopes;
    bool first = true;
    for (const auto &activity : activities) {
        if (!first) scopes += ' ';
        scopes += activity;
        scopes += ':';
        scopes.append(scope_path);
        first = false;
    }

    body = "grant_type=client_credentials&expire_in=" +
        std::to_string(validity * 60) + "&scopes=" + PercentEncode(scopes);
    return true;
}

bool
XrdClHttp::ParseJsonStringResponse(std::string_view input,
                                   std::string_view key, std::string &value,
                                   std::string &error)
{
    value.clear();
    error.clear();
    if (input.size() >= kMaxTokenResponseSize) {
        error = "Token response exceeds maximum size";
        return false;
    }
    if (input.empty()) {
        error = "Token response contained no data";
        return false;
    }
    if (key.empty()) {
        error = "Token response key must not be empty";
        return false;
    }

    auto parsed = nlohmann::json::parse(input.begin(), input.end(), nullptr,
                                        false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        error = "Token response was not valid JSON";
        return false;
    }

    auto member = parsed.find(std::string(key));
    if (member == parsed.end() || !member->is_string() ||
        member->get_ref<const std::string &>().empty()) {
        error = "Token response did not include a non-empty string '" +
            std::string(key) + "'";
        return false;
    }
    value = member->get<std::string>();
    return true;
}

bool
XrdClHttp::ParseMacaroonResponse(std::string_view input, std::string &token,
                                 std::string &error)
{
    return ParseJsonStringResponse(input, "macaroon", token, error);
}

/******************************************************************************/
/* Copyright (C) 2026, XRootD Collaboration                                  */
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

#include "XrdClHttpWebDav.hh"
#include "XrdClHttpUtil.hh"

#include <tinyxml.h>

#include <charconv>
#include <cstring>

using namespace XrdClHttp;

namespace {

bool WebDavStatusIsSuccessful(const char *status)
{
    if (!status) return true;
    auto value = trim_view(status);
    auto code_begin = value.find(' ');
    if (code_begin == std::string_view::npos) return false;
    code_begin = value.find_first_not_of(' ', code_begin);
    if (code_begin == std::string_view::npos) return false;
    auto code_end = value.find(' ', code_begin);
    auto code_limit = value.data() +
        (code_end == std::string_view::npos ? value.size() : code_end);
    int code = 0;
    auto result = std::from_chars(value.data() + code_begin, code_limit, code);
    return result.ec == std::errc() && result.ptr == code_limit &&
        code >= 200 && code < 300;
}

}

bool
XrdClHttp::WebDavElementNameEquals(const TiXmlElement *element,
                                   const char *expected)
{
    if (!element || !element->Value()) return false;
    const char *name = element->Value();
    const char *separator = std::strrchr(name, ':');
    return !strcasecmp(separator ? separator + 1 : name, expected);
}

bool
XrdClHttp::ParseWebDavProperties(TiXmlElement *prop,
                                 WebDavProperties &properties)
{
    if (!prop) return false;

    bool has_size = false;
    for (auto child = prop->FirstChildElement(); child != nullptr;
         child = child->NextSiblingElement()) {
        if (WebDavElementNameEquals(child, "resourcetype")) {
            for (auto type = child->FirstChildElement(); type != nullptr;
                 type = type->NextSiblingElement()) {
                if (WebDavElementNameEquals(type, "collection")) {
                    properties.m_is_dir = true;
                    break;
                }
            }
        } else if (WebDavElementNameEquals(child, "getcontentlength")) {
            auto text = child->GetText();
            if (!text) continue;

            auto value = trim_view(text);
            if (value.empty()) continue;

            int64_t size = -1;
            auto result = std::from_chars(value.data(),
                                          value.data() + value.size(), size);
            if (result.ec != std::errc() ||
                result.ptr != value.data() + value.size() || size < 0) {
                return false;
            }
            properties.m_size = size;
            has_size = true;
        } else if (WebDavElementNameEquals(child, "getlastmodified")) {
            auto last_modified = child->GetText();
            if (!last_modified) return false;
            properties.m_last_modified_text = last_modified;
            struct tm parsed_time{};
            if (!strptime(last_modified, "%a, %d %b %Y %H:%M:%S",
                          &parsed_time)) {
                return false;
            }
            properties.m_last_modified = timegm(&parsed_time);
        } else if (WebDavElementNameEquals(child, "executable")) {
            auto executable = child->GetText();
            if (!executable) return false;
            properties.m_is_executable = !strcasecmp(executable, "T");
        }
    }

    if (properties.m_is_dir) {
        if (!has_size) properties.m_size = 0;
        return true;
    }
    return has_size;
}

bool
XrdClHttp::ParseWebDavResponseProperties(TiXmlElement *response,
                                         WebDavProperties &properties)
{
    if (!response) return false;

    for (auto propstat = response->FirstChildElement(); propstat != nullptr;
         propstat = propstat->NextSiblingElement()) {
        if (!WebDavElementNameEquals(propstat, "propstat")) continue;

        TiXmlElement *prop = nullptr;
        const char *status = nullptr;
        for (auto child = propstat->FirstChildElement(); child != nullptr;
             child = child->NextSiblingElement()) {
            if (WebDavElementNameEquals(child, "prop")) {
                prop = child;
            } else if (WebDavElementNameEquals(child, "status")) {
                status = child->GetText();
            }
        }

        if (!WebDavStatusIsSuccessful(status)) continue;

        if (!prop) return false;
        return ParseWebDavProperties(prop, properties);
    }
    return false;
}

bool
XrdClHttp::ParseWebDavResponseQuota(TiXmlElement *response, WebDavQuota &quota)
{
    if (!response) return false;
    for (auto propstat = response->FirstChildElement(); propstat != nullptr;
         propstat = propstat->NextSiblingElement()) {
        if (!WebDavElementNameEquals(propstat, "propstat")) continue;
        TiXmlElement *prop = nullptr;
        const char *status = nullptr;
        for (auto child = propstat->FirstChildElement(); child != nullptr;
             child = child->NextSiblingElement()) {
            if (WebDavElementNameEquals(child, "prop")) prop = child;
            if (WebDavElementNameEquals(child, "status")) status = child->GetText();
        }
        if (!WebDavStatusIsSuccessful(status) || !prop) continue;
        bool has_available = false;
        bool has_used = false;
        for (auto child = prop->FirstChildElement(); child != nullptr;
             child = child->NextSiblingElement()) {
            auto text = child->GetText();
            if (!text) continue;
            auto value = trim_view(text);
            uint64_t parsed = 0;
            auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc() || result.ptr != value.data() + value.size())
                return false;
            if (WebDavElementNameEquals(child, "quota-available-bytes")) {
                quota.m_available = parsed;
                has_available = true;
            } else if (WebDavElementNameEquals(child, "quota-used-bytes")) {
                quota.m_used = parsed;
                has_used = true;
            }
        }
        if (has_available && has_used) return true;
    }
    return false;
}

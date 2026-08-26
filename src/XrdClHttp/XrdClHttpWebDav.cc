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

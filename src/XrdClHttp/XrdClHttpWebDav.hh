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

#ifndef XRDCLHTTPWEBDAV_HH
#define XRDCLHTTPWEBDAV_HH

#include <cstdint>
#include <ctime>

class TiXmlElement;

namespace XrdClHttp {

struct WebDavProperties {
    bool m_is_dir{false};
    bool m_is_executable{false};
    int64_t m_size{-1};
    time_t m_last_modified{-1};
};

// Compare an XML element's local name, ignoring any namespace prefix.
bool WebDavElementNameEquals(const TiXmlElement *element,
                             const char *expected);

// Parse the standard properties returned by a WebDAV PROPFIND response.
// Collections may omit their content length; regular resources may not.
bool ParseWebDavProperties(TiXmlElement *prop, WebDavProperties &properties);

}

#endif

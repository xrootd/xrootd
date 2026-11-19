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

#include "XrdClHttp/XrdClHttpUtil.hh"
#include "XrdClHttp/XrdClHttpWebDav.hh"

#include <gtest/gtest.h>
#include <tinyxml.h>

namespace {

bool ParseProperties(const char *xml, XrdClHttp::WebDavProperties &properties)
{
    TiXmlDocument document;
    document.Parse(xml);
    return !document.Error() &&
        XrdClHttp::ParseWebDavProperties(document.RootElement(), properties);
}

bool ParseResponseProperties(const char *xml,
                             XrdClHttp::WebDavProperties &properties)
{
    TiXmlDocument document;
    document.Parse(xml);
    return !document.Error() && XrdClHttp::ParseWebDavResponseProperties(
        document.RootElement(), properties);
}

}

TEST(WebDavParser, ParsesWhitespaceSeparatedAllowMethods)
{
    XrdClHttp::HeaderParser parser;
    EXPECT_TRUE(parser.Parse("HTTP/1.1 200 OK\r\n"));
    EXPECT_TRUE(parser.Parse("Allow: MKCOL, PROPFIND, PUT\r\n"));
    EXPECT_TRUE(parser.Parse("\r\n"));

    EXPECT_TRUE(parser.GetAllowedVerbs().IsSet(
        XrdClHttp::VerbsCache::HttpVerb::kPROPFIND));
}

TEST(WebDavParser, MatchesLocalNamesWithArbitraryPrefixes)
{
    TiXmlDocument document;
    document.Parse("<ns0:collection xmlns:ns0=\"DAV:\"/>");

    EXPECT_TRUE(XrdClHttp::WebDavElementNameEquals(
        document.RootElement(), "collection"));
    EXPECT_FALSE(XrdClHttp::WebDavElementNameEquals(
        document.RootElement(), "resourcetype"));
}

TEST(WebDavParser, AllowsDirectoryWithEmptyContentLength)
{
    XrdClHttp::WebDavProperties properties;
    ASSERT_TRUE(ParseProperties(
        "<d:prop xmlns:d=\"DAV:\">"
        "<d:getcontentlength/>"
        "<d:resourcetype><d:collection/></d:resourcetype>"
        "</d:prop>", properties));

    EXPECT_TRUE(properties.m_is_dir);
    EXPECT_EQ(properties.m_size, 0);
}

TEST(WebDavParser, RequiresContentLengthForRegularResources)
{
    XrdClHttp::WebDavProperties missing;
    EXPECT_FALSE(ParseProperties(
        "<d:prop xmlns:d=\"DAV:\"><d:resourcetype/></d:prop>", missing));

    XrdClHttp::WebDavProperties empty;
    EXPECT_FALSE(ParseProperties(
        "<d:prop xmlns:d=\"DAV:\"><d:getcontentlength/></d:prop>", empty));
}

TEST(WebDavParser, ParsesValidRegularResourceSize)
{
    XrdClHttp::WebDavProperties properties;
    ASSERT_TRUE(ParseProperties(
        "<d:prop xmlns:d=\"DAV:\">"
        "<d:getcontentlength> 123 </d:getcontentlength>"
        "<d:resourcetype/>"
        "</d:prop>", properties));

    EXPECT_FALSE(properties.m_is_dir);
    EXPECT_EQ(properties.m_size, 123);
}

TEST(WebDavParser, RejectsInvalidContentLength)
{
    for (auto value : {"-1", "12 bytes", "9223372036854775808"}) {
        XrdClHttp::WebDavProperties properties;
        std::string xml = "<d:prop xmlns:d=\"DAV:\"><d:getcontentlength>";
        xml += value;
        xml += "</d:getcontentlength></d:prop>";
        EXPECT_FALSE(ParseProperties(xml.c_str(), properties)) << value;
    }
}

TEST(WebDavParser, IgnoresUnsuccessfulPropstatEntries)
{
    XrdClHttp::WebDavProperties properties;
    ASSERT_TRUE(ParseResponseProperties(
        "<d:response xmlns:d=\"DAV:\">"
        "<d:propstat>"
        "<d:status>HTTP/1.1 200 OK</d:status>"
        "<d:prop><d:getcontentlength>123</d:getcontentlength></d:prop>"
        "</d:propstat>"
        "<d:propstat>"
        "<d:status>HTTP/1.1 404 Not Found</d:status><d:prop/>"
        "</d:propstat>"
        "</d:response>", properties));

    EXPECT_EQ(properties.m_size, 123);
}

TEST(WebDavParser, RequiresSuccessfulPropstatEntry)
{
    XrdClHttp::WebDavProperties properties;
    EXPECT_FALSE(ParseResponseProperties(
        "<d:response xmlns:d=\"DAV:\">"
        "<d:propstat>"
        "<d:status>HTTP/1.1 404 Not Found</d:status><d:prop/>"
        "</d:propstat>"
        "</d:response>", properties));
}

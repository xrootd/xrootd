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

#include <gtest/gtest.h>

TEST(WebDavParser, ParsesWhitespaceSeparatedAllowMethods)
{
    XrdClHttp::HeaderParser parser;
    EXPECT_TRUE(parser.Parse("HTTP/1.1 200 OK\r\n"));
    EXPECT_TRUE(parser.Parse("Allow: MKCOL, PROPFIND, PUT\r\n"));
    EXPECT_TRUE(parser.Parse("\r\n"));

    EXPECT_TRUE(parser.GetAllowedVerbs().IsSet(
        XrdClHttp::VerbsCache::HttpVerb::kPROPFIND));
}

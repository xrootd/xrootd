/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
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

#include "XrdClHttp/XrdClHttpParseTimeout.hh"

#include <gtest/gtest.h>

using namespace XrdClHttp;

TEST(ParseTimeout, BasicHr) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("1h", ts, err);
    EXPECT_TRUE(result);
    EXPECT_EQ(err, "");
    EXPECT_EQ(ts.tv_sec, 3600);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST(ParseTimeout, BasicFraction) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("1.5s", ts, err);
    EXPECT_TRUE(result);
    EXPECT_EQ(err, "");
    EXPECT_EQ(ts.tv_sec, 1);
    EXPECT_EQ(ts.tv_nsec, 500'000'000);
}

TEST(ParseTimeout, Compound) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("1s50ms", ts, err);
    EXPECT_TRUE(result);
    EXPECT_EQ(err, "");
    EXPECT_EQ(ts.tv_sec, 1);
    EXPECT_EQ(ts.tv_nsec, 50'000'000);
}

TEST(ParseTimeout, Negative) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("-10ms", ts, err);
    EXPECT_FALSE(result);
    EXPECT_EQ(err, "Provided timeout was negative");
    EXPECT_EQ(ts.tv_sec, 0);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST(ParseTimeout, NoUnit) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("3.3", ts, err);
    EXPECT_FALSE(result);
    EXPECT_EQ(err, "Unit missing from duration: 3.3");
    EXPECT_EQ(ts.tv_sec, 0);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST(ParseTimeout, Empty) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("", ts, err);
    EXPECT_FALSE(result);
    EXPECT_EQ(err, "cannot parse empty string as a time duration");
    EXPECT_EQ(ts.tv_sec, 0);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST(ParseTimeout, Zero) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("0", ts, err);
    EXPECT_TRUE(result);
    EXPECT_EQ(err, "");
    EXPECT_EQ(ts.tv_sec, 0);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST(ParseTimeout, Invalid) {
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 0;
    std::string err;
    auto result = ParseTimeout("foo", ts, err);
    EXPECT_FALSE(result);
    EXPECT_EQ(err, "Invalid number provided as timeout: foo");
    EXPECT_EQ(ts.tv_sec, 0);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST(MarshalDuration, Zero) {
    struct timespec ts = {0, 0};
    auto result = MarshalDuration(ts);
    EXPECT_NE(result, "");
    EXPECT_EQ(result, "0s");
}

TEST(MarshalDuration, Simple) {
    struct timespec ts = {0, 500'000'000};
    auto result = MarshalDuration(ts);
    EXPECT_EQ(result, "500ms");
    ts.tv_sec = 1;
    result = MarshalDuration(ts);
    EXPECT_EQ(result, "1s500ms");
}

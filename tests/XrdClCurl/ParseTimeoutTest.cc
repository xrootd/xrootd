/****************************************************************
 *
 * Copyright (C) 2024, Pelican Project, Morgridge Institute for Research
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

#include "XrdClCurl/XrdClCurlParseTimeout.hh"

#include <gtest/gtest.h>

using namespace XrdClCurl;

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

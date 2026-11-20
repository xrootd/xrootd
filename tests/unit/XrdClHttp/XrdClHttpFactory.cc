/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/******************************************************************************/

#include "XrdClHttp/XrdClHttpFactory.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClEnv.hh"

#include <gtest/gtest.h>

#include <map>

using namespace XrdClHttp;

extern "C" void *XrdClGetPlugIn(const void *arg);

TEST(Factory, PluginConfig) {
    std::map<std::string, std::string> config{
        {"url", "https://*"},
        {"lib", "libXrdClHttp.so"},
        {"enable", "true"},
        {"httpstalltimeout", "42"},
        {"httpcertfile", "/etc/ssl/certs/ca-bundle.crt"},
    };

    auto factory = static_cast<Factory *>(XrdClGetPlugIn(&config));
    ASSERT_NE(factory, nullptr);

    auto fs = factory->CreateFileSystem("https://example.com/");
    ASSERT_NE(fs, nullptr);
    delete fs;

    int stall_timeout = 0;
    ASSERT_TRUE(XrdCl::DefaultEnv::GetEnv()->GetInt("HttpStallTimeout", stall_timeout));
    ASSERT_EQ(stall_timeout, 42);

    std::string cert_file;
    ASSERT_TRUE(XrdCl::DefaultEnv::GetEnv()->GetString("HttpCertFile", cert_file));
    ASSERT_EQ(cert_file, "/etc/ssl/certs/ca-bundle.crt");

    delete factory;
}

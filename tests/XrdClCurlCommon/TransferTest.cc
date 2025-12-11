/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClCurl client plugin for XRootD.               */
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

#include "TransferTest.hh"

#include "XrdClCurl/XrdClCurlFile.hh"

#include <curl/curl.h>
#include <XrdCl/XrdClConstants.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>

#include <fstream>

std::once_flag TransferFixture::m_init;
bool TransferFixture::m_initialized = false;
std::unordered_map<std::string, std::string> TransferFixture::m_env;
std::string TransferFixture::m_cache_url;
XrdCl::Log *TransferFixture::m_log = nullptr;
std::string TransferFixture::m_read_token_location;
std::string TransferFixture::m_read_token;
std::string TransferFixture::m_write_token_location;
std::string TransferFixture::m_write_token;
std::string TransferFixture::m_ca_file;
std::string TransferFixture::m_origin_url;

namespace {

// Trim the left side of a string_view for space
std::string_view ltrim_view(const std::string_view &input_view) {
    for (size_t idx = 0; idx < input_view.size(); idx++) {
        if (!isspace(input_view[idx])) {
            return input_view.substr(idx);
        }
    }
    return "";
}

// Trim left and righit side of a string_view for space characters
std::string_view trim_view(const std::string_view &input_view) {
    auto view = ltrim_view(input_view);
    for (size_t idx = 0; idx < input_view.size(); idx++) {
        if (!isspace(view[view.size() - 1 - idx])) {
            return view.substr(0, view.size() - idx);
        }
    }
    return "";
}

}
TransferFixture::TransferFixture()
    {}

void
TransferFixture::ForkChild()
{
    m_log = XrdCl::DefaultEnv::GetLog();
}

const std::string
TransferFixture::GetEnv(const std::string &key) const {
    auto iter = m_env.find(key);
    if (iter == m_env.end()) return "";
    else return iter->second;
}

void TransferFixture::SetUp() {
    std::call_once(m_init, [&] {
        m_log = XrdCl::DefaultEnv::GetLog();
        char *env_file = getenv("ENV_FILE");

        ASSERT_NE(env_file, nullptr) << "$ENV_FILE environment variable "
                                        "not set; required to run test; this variable is set "
                                        "automatically when test is run via `ctest`.";
        parseEnvFile(env_file);

        m_initialized = true;
        pthread_atfork(nullptr, nullptr, ForkChild);
    });
    ASSERT_TRUE(m_initialized) << "Environment initialization failed";
}

void
TransferFixture::WritePattern(const std::string &name, const off_t writeSize,
                              const unsigned char chunkByte, const size_t chunkSize)
{
    XrdCl::File fh;

    auto url = name + "?authz=" + GetWriteToken() + "&oss.asize=" + std::to_string(writeSize);
    auto rv = fh.Open(url, XrdCl::OpenFlags::Write, XrdCl::Access::Mode(0755), static_cast<time_t>(0));
    ASSERT_TRUE(rv.IsOK()) << "Failed to open " << name << " for write: " << rv.ToString();

    size_t sizeToWrite = (static_cast<off_t>(chunkSize) >= writeSize)
                                                        ? static_cast<size_t>(writeSize)
                                                        : chunkSize;
    off_t curWriteSize = writeSize;
    auto curChunkByte = chunkByte;
    off_t offset = 0;
    while (sizeToWrite) {
        std::string writeBuffer(sizeToWrite, curChunkByte);

        rv = fh.Write(offset, sizeToWrite, writeBuffer.data(), static_cast<time_t>(10));
        ASSERT_TRUE(rv.IsOK()) << "Failed to write " << name << ": " << rv.ToString();
        
        curWriteSize -= sizeToWrite;
        offset += sizeToWrite;
        sizeToWrite = (static_cast<off_t>(chunkSize) >= curWriteSize)
                                            ? static_cast<size_t>(curWriteSize)
                                            : chunkSize;
        curChunkByte += 1;
    }

    rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());
    m_log->Debug(XrdCl::UtilityMsg, "Finished writing transfer pattern to %s", name.c_str());

    VerifyContents(name, writeSize, chunkByte, chunkSize);
}

void
TransferFixture::VerifyContents(const std::string &obj,
                                off_t expectedSize, unsigned char chunkByte,
                                size_t chunkSize) {
    XrdCl::File fh;
    auto url = obj + "?authz=" + GetReadToken();
    auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755), static_cast<time_t>(0));
    ASSERT_TRUE(rv.IsOK());

    return VerifyContents(fh, expectedSize, chunkByte, chunkSize);
}

void
TransferFixture::VerifyContents(XrdCl::File &fh,
                                off_t expectedSize, unsigned char chunkByte,
                                size_t chunkSize) {
        
    size_t sizeToRead = (static_cast<off_t>(chunkSize) >= expectedSize)
                                                    ? expectedSize
                                                    : chunkSize;
    unsigned char curChunkByte = chunkByte;
    off_t offset = 0;
    while (sizeToRead) {
        std::string readBuffer(sizeToRead, curChunkByte - 1);
        uint32_t bytesRead;
        auto rv = fh.Read(offset, sizeToRead, readBuffer.data(), bytesRead, static_cast<time_t>(0));
        ASSERT_TRUE(rv.IsOK());
        ASSERT_EQ(bytesRead, sizeToRead);
        readBuffer.resize(sizeToRead);

        std::string correctBuffer(sizeToRead, curChunkByte);
        ASSERT_EQ(readBuffer, correctBuffer);

        expectedSize -= sizeToRead;
        offset += sizeToRead;
        sizeToRead = (static_cast<off_t>(chunkSize) >= expectedSize)
                                            ? expectedSize
                                            : chunkSize;
        curChunkByte += 1;
    }

    auto rv = fh.Close();
    ASSERT_TRUE(rv.IsOK());
}

void
TransferFixture::ReadTokenFromFile(const std::string &fname, std::string &token)
{
    std::ifstream fh(fname);
    ASSERT_TRUE(fh.is_open());
    std::string line;
    while (std::getline(fh, line)) {
        auto contents = trim_view(line);
        if (contents.empty()) {continue;}
        if (contents[0] == '#') {continue;}
        token = contents;
        return;
    }
    ASSERT_FALSE(fh.fail());
    FAIL() << "No token found in file " << fname;
}

void
TransferFixture::parseEnvFile(const std::string &fname) {
    std::ifstream fh(fname);
    if (!fh.is_open()) {
        std::cerr << "Failed to open env file: " << strerror(errno);
        exit(1);
    }
    std::string line;
    while (std::getline(fh, line)) {
        auto idx = line.find("=");
        if (idx == std::string::npos) {
            continue;
        }
        auto key = line.substr(0, idx);
        auto val = line.substr(idx + 1);
        m_env[key] = val;

        if (key == "CACHE_URL") {
            m_cache_url = val;
        } else if (key == "X509_CA_FILE") {
            m_ca_file = val;
            setenv("X509_CERT_FILE", m_ca_file.c_str(), 1);
        } else if (key == "ORIGIN_URL") {
            m_origin_url = val;
        } else if (key == "WRITE_TOKEN") {
            m_write_token_location = val;
            ReadTokenFromFile(m_write_token_location, m_write_token);
        } else if (key == "READ_TOKEN") {
            m_read_token_location = val;
            ReadTokenFromFile(m_read_token_location, m_read_token);
        } else if (key == "XRD_PLUGINCONFDIR") {
            setenv("XRD_PLUGINCONFDIR", val.c_str(), 1);
        }
    }
}

void
TransferFixture::SyncResponseHandler::HandleResponse( XrdCl::XRootDStatus *status, XrdCl::AnyObject *response ) {
    std::unique_lock lock(m_mutex);
    m_status.reset(status);
    m_obj.reset(response);
    m_cv.notify_one();
}

void
TransferFixture::SyncResponseHandler::Wait() {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [&]{return m_status.get() != nullptr;});
}

std::tuple<std::unique_ptr<XrdCl::XRootDStatus>, std::unique_ptr<XrdCl::AnyObject>>
TransferFixture::SyncResponseHandler::Status() {
    return std::make_tuple(std::move(m_status), std::move(m_obj));
}

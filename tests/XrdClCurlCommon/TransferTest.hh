/****************************************************************
 *
 * Copyright (C) 2025, Pelican Project, Morgridge Institute for Research
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

#ifndef XRDCLCURL_TRANSFERTEST_HH
#define XRDCLCURL_TRANSFERTEST_HH

#include "XrdCl/XrdClXRootDResponses.hh"

#include <gtest/gtest.h>

#include <mutex>
#include <condition_variable>

namespace XrdClCurl {
    class Factory;
}

namespace XrdCl {
    class AnyObject;
    class File;
    class Log;
    class XRootDStatus;
}

// A common class that contains utilities for interacting with the
// running origin test fixture launched by ctest
class TransferFixture : public testing::Test {

    protected:
        TransferFixture();
    
        void SetUp() override;
    
        // Helper function to write a file with a given pattern of contents.
        //
        // Useful for making large files with known contents that are varying from
        // call-to-call (useful in detection of off-by-one errors).
        // - `writeSize`: Total size of the uploaded object
        // - `chunkByte`: Byte value of the first character in the file.
        // - `chunkSize`: Number of bytes to repeat the first `chunkByte`.  Afterward,
        //   the function will write `chunkByte + 1` for `chunkSize` characters and so on.
        void WritePattern(const std::string &name, const off_t writeSize,
                          const unsigned char chunkByte, const size_t chunkSize);
    
        // Helper function to verify the contents of a given object..
        //
        // Useful for verifying the contents of an object created with `WritePattern`
        // - `writeSize`: Total size of the uploaded object
        // - `chunkByte`: Byte value of the first character in the file.
        // - `chunkSize`: Number of bytes to repeat the first `chunkByte`.  Afterward,
        //   the function will write `chunkByte + 1` for `chunkSize` characters and so on.
        void VerifyContents(const std::string &name, const off_t writeSize,
                            const unsigned char chunkByte, const size_t chunkSize);

        // A variant of VerifyContents that takes in an open file handle instead of the
        // object name.
        void VerifyContents(XrdCl::File &fh, const off_t writeSize,
                            const unsigned char chunkByte, const size_t chunkSize);
    
        const std::string &GetCacheURL() const {return m_cache_url;}
        const std::string &GetOriginURL() const {return m_origin_url;}
        const std::string &GetReadToken() const {return m_read_token;}
        const std::string &GetWriteToken() const {return m_write_token;}
    
        // Convenience handler to block until an operation has completed
        class SyncResponseHandler: public XrdCl::ResponseHandler {
            public:
                SyncResponseHandler() {}
            
                virtual ~SyncResponseHandler() {}
            
                virtual void HandleResponse( XrdCl::XRootDStatus *status, XrdCl::AnyObject *response );
            
                void Wait();
            
                std::tuple<std::unique_ptr<XrdCl::XRootDStatus>, std::unique_ptr<XrdCl::AnyObject>> Status();
            
            private:
                std::mutex m_mutex;
                std::condition_variable m_cv;
            
                std::unique_ptr<XrdCl::XRootDStatus> m_status;
                std::unique_ptr<XrdCl::AnyObject> m_obj;
        };

        // Set the cache URL
        void SetCacheUrl(const std::string &url) {m_cache_url = url;}

        // Retrieve a configuration value from the environment file
        const std::string GetEnv(const std::string &) const;

    private:
        void ReadTokenFromFile(const std::string &fname, std::string &token);
    
        // Function to reinitialize the fixture after fork() has been called
        static void ForkChild();

        // Flag for initializing the global settings inherited from the text fixture.
        static std::once_flag m_init;

        // URL for contacting the caches.
        // This can be overridden via `SetCacheUrl` to have a separate endpoint contacted
        static std::string m_cache_url;

        // Environment variables from the test fixture
        static std::unordered_map<std::string, std::string> m_env;

        // Log object to help debug test runs
        static XrdCl::Log *m_log;
    
        // Whether the test fixture globals were initialized
        static bool m_initialized;
    
        // Location of the read token file
        static std::string m_read_token_location;
    
        // Contents of the read token
        static std::string m_read_token;
    
        // Location of the write token file
        static std::string m_write_token_location;
    
        // Contents of the write token
        static std::string m_write_token;
    
        // Location of the custom CA file used by the test
        static std::string m_ca_file;
    
        // URL prefix to contact the origin
        static std::string m_origin_url;
    
        void parseEnvFile(const std::string &fname);
};

#endif // XRDCLCURL_TRANSFERTEST_HH

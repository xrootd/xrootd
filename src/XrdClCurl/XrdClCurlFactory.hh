/***************************************************************
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

#ifndef XRDCLCURL_FACTORY_HH
#define XRDCLCURL_FACTORY_HH

#include "XrdCl/XrdClPlugInInterface.hh"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <time.h>
#include <vector>

namespace XrdCl {
    class Log;
}

namespace XrdClCurl {

class CurlOperation;
class CurlWorker;
class HandlerQueue;

class Factory final : public XrdCl::PlugInFactory {
public:
    Factory() {}

    virtual XrdCl::FilePlugIn *CreateFile(const std::string &url) override;
    virtual XrdCl::FileSystemPlugIn *CreateFileSystem(const std::string &url) override;

    // Get the header timeout value, taking into consideration the provided command timeout, and XrdCl's default values
    static struct timespec GetHeaderTimeoutWithDefault(time_t oper_timeout);

    // Hand off a given curl operation to the factory's worker pool.
    void Produce(std::unique_ptr<XrdClCurl::CurlOperation> operation);

private:
    // Actual initialization of the factory.  Only done when the first filesystem/file
    // is created to allow a parent process to fork first.
    void Initialize();

    // Set the various X509 credential variables in the default environment.
    void SetupX509();

    // Monitoring loop for XrdClCurl statistics
    void Monitor();

    // Invoked by libc when the library is shutting down or is unloaded from the process.
    static void Shutdown() __attribute__((destructor));

    static bool m_initialized;
    static std::shared_ptr<XrdClCurl::HandlerQueue> m_queue;
    static XrdCl::Log *m_log;
    static std::vector<std::unique_ptr<XrdClCurl::CurlWorker>> m_workers;
    const static unsigned m_poll_threads{8};
    static std::once_flag m_init_once;
    // Location for the client to dump its runtime statistics.
    static std::string m_stats_location;

    // Start time of the factory
    static std::chrono::system_clock::time_point m_start;

    // Mutex for managing the shutdown of the background thread
    static std::mutex m_shutdown_lock;
    // Condition variable managing the requested shutdown of the background thread.
    static std::condition_variable m_shutdown_requested_cv;
    // Flag indicating that a shutdown was requested.
    static bool m_shutdown_requested;
    // Condition variable for the background thread to indicate it has completed.
    static std::condition_variable m_shutdown_complete_cv;
    // Flag indicating that the shutdown has completed.
    static bool m_shutdown_complete;
};

}

#endif // XRDCLCURL_FACTORY_HH

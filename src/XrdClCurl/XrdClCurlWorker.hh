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

#ifndef XRDCLCURLWORKER_HH
#define XRDCLCURLWORKER_HH

#include "XrdClCurlOps.hh"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

typedef void CURL;

namespace XrdCl {

class Env;
class Log;
class ResponseHandler;
class URL;

}

namespace XrdClCurl {

class HandlerQueue;
class VerbsCache;

class CurlWorker {
public:
    CurlWorker(std::shared_ptr<HandlerQueue> queue, VerbsCache &cache, XrdCl::Log* logger);

    CurlWorker(const CurlWorker &) = delete;

    void Run();
    static void RunStatic(CurlWorker *myself);

    // Returns the configured X509 client certificate and key file name
    std::tuple<std::string, std::string> ClientX509CertKeyFile() const;

    // Change the period (in seconds) for queue maintenance.
    //
    // Defaults to 5 seconds; smaller values are convenient for unit tests.
    static void SetMaintenancePeriod(unsigned maint) {
        m_maintenance_period.store(maint, std::memory_order_relaxed);
    }

private:
    // Invoked when the plugin is unloaded, triggers the shutdown of each of the worker threads.
    static void ShutdownAll() __attribute__((destructor));

    // Invoked by ShutdownAll, kills off the current object's thread
    void Shutdown();

    // A list of all known worker threads -- used to shutdown the process
    static std::vector<CurlWorker*> m_workers;
    // Protects the data in m_workers
    static std::mutex m_workers_mutex;

    std::chrono::steady_clock::time_point m_last_prefix_log;
    VerbsCache &m_cache; // Cache mapping server URLs to list of selected HTTP verbs.
    std::shared_ptr<HandlerQueue> m_queue;

    // Queue for operations that can be unpaused.
    // Paused operations occur when a PUT is started but cannot be continued
    // because more data is needed from the caller.
    std::shared_ptr<HandlerQueue> m_continue_queue;

    std::unordered_map<CURL*, std::shared_ptr<CurlOperation>> m_op_map;
    XrdCl::Log* m_logger;
    std::string m_x509_client_cert_file;
    std::string m_x509_client_key_file;

    const static unsigned m_max_ops{20};
    static std::atomic<unsigned> m_maintenance_period;

    // File descriptor pair indicating shutdown is requested.
    int m_shutdown_pipe_r{-1};
    int m_shutdown_pipe_w{-1};
    // Mutex for managing the shutdown of the background thread
    std::mutex m_shutdown_lock;
    // Condition variable for the background thread to indicate it has completed.
    std::condition_variable m_shutdown_complete_cv;
    // Flag indicating that the shutdown has completed.
    bool m_shutdown_complete{true};
};

}

#endif // XRDCLCURLWORKER_HH

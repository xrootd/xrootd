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

#ifndef XRDCLHTTPWORKER_HH
#define XRDCLHTTPWORKER_HH

#include "XrdClHttpOps.hh"

#include <array>
#include <atomic>
#include <chrono>
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

namespace XrdClHttp {

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

    static std::string GetMonitoringJson();

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

    std::unordered_map<CURL*, std::pair<std::shared_ptr<CurlOperation>, std::chrono::system_clock::time_point>> m_op_map;
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

    // Monitoring statistics
    struct OpStats {
        std::atomic<uint64_t> m_conncall_timeout{}; // Timeout due to the connection callout mechanism
        std::atomic<uint64_t> m_client_timeout{};
        std::atomic<std::chrono::system_clock::duration::rep> m_duration{};
        std::atomic<uint64_t> m_error{};
        std::atomic<uint64_t> m_finished{};
        std::atomic<std::chrono::steady_clock::duration::rep> m_pause_duration{};
        std::atomic<uint64_t> m_started{};
        std::atomic<uint64_t> m_server_timeout{};
        std::atomic<uint64_t> m_bytes{};
    };

    enum class OpKind {
        ConncallTimeout,
        ClientTimeout,
        Error,
        Finish,
        Start,
        ServerTimeout,
        Update
    };
    void OpRecord(XrdClHttp::CurlOperation &op, OpKind);

    static std::atomic<uint64_t> m_conncall_errors;
    static std::atomic<uint64_t> m_conncall_req;
    static std::atomic<uint64_t> m_conncall_success;
    static std::atomic<uint64_t> m_conncall_timeout;
    static std::array<std::array<OpStats, 403>, static_cast<size_t>(XrdClHttp::CurlOperation::HttpVerb::Count)> m_ops;
    std::atomic<std::chrono::system_clock::rep> m_last_completed_cycle;
    std::atomic<std::chrono::system_clock::rep> m_oldest_op;

    // Vector tracking known worker statistics.
    static std::vector<std::atomic<std::chrono::system_clock::rep>*> m_workers_last_completed_cycle;
    static std::vector<std::atomic<std::chrono::system_clock::rep>*> m_workers_oldest_op;
    size_t m_stats_offset{0};
    static std::mutex m_worker_stats_mutex;
};

}

#endif // XRDCLHTTPWORKER_HH

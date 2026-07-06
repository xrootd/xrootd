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

#include "XrdClHttpFactory.hh"
#include "XrdClHttpFile.hh"
#include "XrdClHttpFilesystem.hh"
#include "XrdClHttpUtil.hh"
#include "XrdClHttpOps.hh"
#include "XrdClHttpParseTimeout.hh"
#include "XrdClHttpWorker.hh"

#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdVersion.hh"

#include <unistd.h>

XrdVERSIONINFO(XrdClGetPlugIn, XrdClGetPlugIn)

using namespace XrdClHttp;

struct timespec
Factory::GetHeaderTimeoutWithDefault(time_t oper_timeout)
{
    if (oper_timeout == 0) {
        int val = XrdCl::DefaultRequestTimeout;
        XrdCl::DefaultEnv::GetEnv()->GetInt( "RequestTimeout", val );
        oper_timeout = val;
    }
    if (oper_timeout <= 0) {
        return {0, 0};
    }
    return {oper_timeout, 0};
}

bool Factory::m_initialized = false;
std::map<std::string, std::string> Factory::m_plugin_config;
std::shared_ptr<XrdClHttp::HandlerQueue> Factory::m_queue;
XrdCl::Log *Factory::m_log = nullptr;
std::once_flag Factory::m_init_once;
std::string Factory::m_stats_location;
std::chrono::system_clock::time_point Factory::m_start{};

std::mutex Factory::m_shutdown_lock;
std::thread Factory::m_monitor_tid;
std::condition_variable Factory::m_shutdown_requested_cv;
bool Factory::m_shutdown_requested = false;

// shutdown trigger, must be last of the static members
Factory::shutdown_s Factory::m_shutdowns;

void
Factory::Initialize()
{
    std::unique_lock lock(m_shutdown_lock);
    if (m_shutdown_requested) {
      return;
    }
    std::call_once(m_init_once, [&] {
        m_log = XrdCl::DefaultEnv::GetLog();
        if (!m_log) {
            return;
        }
        m_log->SetTopicName(kLogXrdClHttp, "XrdClHttp");

        auto env = XrdCl::DefaultEnv::GetEnv();
        if (!env) {
            return;
        }

        SetupX509();

        // The location for the client to write the statistics file; this will be dropped
        // atomically every ~5 seconds and is meant to be complementary to the future g-stream.
        env->ResolveString("HttpStatisticsLocation", "XRD_HTTPSTATISTICSLOCATION", &m_plugin_config, m_stats_location, "");
        if (!m_stats_location.empty()) {
            m_log->Debug(kLogXrdClHttp, "Will write client statistics to %s", m_stats_location.c_str());
        } else {
            m_log->Debug(kLogXrdClHttp, "Not writing client statistics to disk");
        }
        m_start = std::chrono::system_clock::now();

        // The minimum value we will accept from the request for a header timeout.
        // (i.e., the amount of time the plugin will wait to receive headers from the remote server)
        std::string minimum_header_timeout;
        env->ResolveString("HttpMinimumHeaderTimeout", "XRD_HTTPMINIMUMHEADERTIMEOUT", &m_plugin_config, minimum_header_timeout, "");

        // The default value of the header timeout (the amount of time the plugin will wait)
        // to receive headers from the remote server.
        std::string default_header_timeout;
        env->ResolveString("HttpDefaultHeaderTimeout", "XRD_HTTPDEFAULTHEADERTIMEOUT", &m_plugin_config, default_header_timeout, "");

        // The number of pending operations allowed in the global work queue.
        int max_pending = XrdClHttp::HandlerQueue::GetDefaultMaxPendingOps();
        env->ResolveInt("HttpMaxPendingOps", "XRD_HTTPMAXPENDINGOPS", &m_plugin_config, max_pending, XrdClHttp::HandlerQueue::GetDefaultMaxPendingOps());
        if (max_pending <= 0 || max_pending > 10'000'000) {
            m_log->Error(kLogXrdClHttp,
                "Invalid value for the maximum number of pending operations in the global work queue (%d); using default value of %d",
                max_pending,
                XrdClHttp::HandlerQueue::GetDefaultMaxPendingOps());
            max_pending = XrdClHttp::HandlerQueue::GetDefaultMaxPendingOps();
            env->PutInt("HttpMaxPendingOps", max_pending);
        }
        m_log->Debug(kLogXrdClHttp, "Using %d pending operations in the global work queue", max_pending);
        m_queue.reset(new XrdClHttp::HandlerQueue(max_pending));

        // The number of threads to use for curl operations.
        int num_threads = m_poll_threads;
        env->ResolveInt("HttpNumThreads", "XRD_HTTPNUMTHREADS", &m_plugin_config, num_threads, m_poll_threads);
        if (num_threads <= 0 || num_threads > 1'000) {
            m_log->Error(kLogXrdClHttp, "Invalid value for the number of threads to use for curl operations (%d); using default value of %d", num_threads, m_poll_threads);
            num_threads = m_poll_threads;
            env->PutInt("HttpNumThreads", num_threads);
        }
        m_log->Debug(kLogXrdClHttp, "Using %d threads for curl operations", num_threads);

        // The stall timeout to use for transfer operations.
        int stall_timeout = XrdClHttp::CurlOperation::GetDefaultStallTimeout();
        env->ResolveInt("HttpStallTimeout", "XRD_HTTPSTALLTIMEOUT", &m_plugin_config, stall_timeout, XrdClHttp::CurlOperation::GetDefaultStallTimeout());
        if (stall_timeout < 0 || stall_timeout > 86'400) {
            m_log->Error(kLogXrdClHttp, "Invalid value for the stall timeout (%d); using default value of %d", stall_timeout, XrdClHttp::CurlOperation::GetDefaultStallTimeout());
            stall_timeout = XrdClHttp::CurlOperation::GetDefaultStallTimeout();
            env->PutInt("HttpStallTimeout", stall_timeout);
        }
        m_log->Debug(kLogXrdClHttp, "Using %d seconds for the stall timeout", stall_timeout);
        XrdClHttp::CurlOperation::SetStallTimeout(stall_timeout);

        // The slow transfer rate, in bytes per second, for timing out slow uploads/downloads.
        int slow_xfer_rate = XrdClHttp::CurlOperation::GetDefaultSlowRateBytesSec();
        env->ResolveInt("HttpSlowRateBytesSec", "XRD_HTTPSLOWRATEBYTESSEC", &m_plugin_config, slow_xfer_rate, XrdClHttp::CurlOperation::GetDefaultSlowRateBytesSec());
        if (slow_xfer_rate < 0 || slow_xfer_rate > (1024 * 1024 * 1024)) {
            m_log->Error(kLogXrdClHttp, "Invalid value for the slow transfer rate threshold (%d); using default value of %d", slow_xfer_rate, XrdClHttp::CurlOperation::GetDefaultSlowRateBytesSec());
            slow_xfer_rate = XrdClHttp::CurlOperation::GetDefaultSlowRateBytesSec();
            env->PutInt("HttpSlowRateBytesSec", slow_xfer_rate);
        }
        m_log->Debug(kLogXrdClHttp, "Using %d bytes/sec for the slow transfer rate threshold", slow_xfer_rate);
        XrdClHttp::CurlOperation::SetSlowRateBytesSec(slow_xfer_rate);

        // Determine the minimum header timeout.  It's somewhat arbitrarily defaulted to 2s; below
        // that and timeouts could be caused by OS scheduling noise.  If the client has unreasonable
        // expectations of the origin, we don't want to cause it to generate lots of origin-side load.
        struct timespec mct{2, 0};
        if (!minimum_header_timeout.empty()) {
            std::string errmsg;
            if (!ParseTimeout(minimum_header_timeout, mct, errmsg)) {
                m_log->Error(kLogXrdClHttp, "Failed to parse the minimum client timeout (%s): %s", minimum_header_timeout.c_str(), errmsg.c_str());
            }
        }
        XrdClHttp::File::SetMinimumHeaderTimeout(mct);

        struct timespec dht{9, 500'000'000};
        if (!default_header_timeout.empty()) {
            std::string errmsg;
            if (!ParseTimeout(default_header_timeout, dht, errmsg)) {
                m_log->Error(kLogXrdClHttp, "Failed to parse the default header timeout (%s): %s", default_header_timeout.c_str(), errmsg.c_str());
            }
        }
        XrdClHttp::File::SetDefaultHeaderTimeout(dht);

        // Start up the cache for the OPTIONS response
        auto &cache = XrdClHttp::VerbsCache::Instance();

        // Startup curl workers after we've set the configs to avoid race conditions
        for (int idx=0; idx<num_threads; idx++) {
            auto wk = std::make_unique<XrdClHttp::CurlWorker>(m_queue, cache, m_log);
            auto wkp = wk.get();
            std::thread t(XrdClHttp::CurlWorker::RunStatic, wkp);
            wkp->Start(std::move(wk), std::move(t));
        }

        std::thread t([this]{Monitor();});
        m_monitor_tid = std::move(t);

        m_initialized = true;
    });
}

void
Factory::Monitor()
{
    // This function is run in a separate thread to monitor the XrdClHttp statistics.
    // It periodically saves the statistics to the stats file.
    // Note: this previously had support for sending the statistics through the gstream.
    // However, this was removed because gstream currently requires linking against XrdServer
    // which is not available in the client; some further rearranging of headers and linkages
    // is necessary.

    while (true) {
        {
            std::unique_lock lock(m_shutdown_lock);
            m_shutdown_requested_cv.wait_for(
                lock,
                std::chrono::seconds(5),
                []{return m_shutdown_requested;}
            );
            if (m_shutdown_requested) {
                break;
            }
        }

        auto now = std::chrono::system_clock::now();

        std::string monitoring = "{\"event\": \"xrdclhttp\", "
            "\"start\": " + std::to_string(std::chrono::duration<double>(m_start.time_since_epoch()).count()) + ","
            "\"now\": " + std::to_string(std::chrono::duration<double>(now.time_since_epoch()).count()) + ","
            "\"file\": " + File::GetMonitoringJson() + ","
            "\"workers\": " + CurlWorker::GetMonitoringJson() + ","
            "\"queues\": " + HandlerQueue::GetMonitoringJson() +
            " }";
        m_log->Info(kLogXrdClHttp, "Client monitoring statistics: %s", monitoring.c_str());
        if (!m_stats_location.empty())
        {
            auto stats_tmp = m_stats_location + ".XXXXXX";
            std::vector<char> stats_vector(stats_tmp.size() + 1, '\0');
            memcpy(&stats_vector[0], stats_tmp.data(), stats_tmp.size() + 1);
            auto fd = mkstemp(&stats_vector[0]);
            if (fd == -1) {
                m_log->Warning(kLogXrdClHttp, "Failed to create temporary stats file %s: %s", m_stats_location.c_str(), strerror(errno));
                continue;
            }
            auto nb = write(fd, monitoring.data(), monitoring.size());
            if (nb != static_cast<ssize_t>(monitoring.size())) {
                if (nb == -1) m_log->Warning(kLogXrdClHttp, "Failed to write statistics into temporary file %s: %s", &stats_vector[0], strerror(errno));
                else m_log->Warning(kLogXrdClHttp, "Failed to write statistics into temporary file %s: short write", &stats_vector[0]);
                close(fd);
                continue;
            }
            close(fd);
            auto rv = rename(&stats_vector[0], m_stats_location.c_str());
            if (rv) {
                m_log->Warning(kLogXrdClHttp, "Failed to atomically rename stats file to final destination %s: %s", m_stats_location.c_str(), strerror(errno));
            }
        }
    }
}

namespace {

void LogIfSet(XrdCl::Log &log, const std::string &optName, const std::string &val) {
    if (!val.empty()) {
        log.Info(kLogXrdClHttp, "Setting %s to value '%s'", optName.c_str(), val.c_str());
    }
}

} // namespace

void
Factory::SetPluginConfig(const std::map<std::string, std::string> &config)
{
    m_plugin_config = config;
}

void
Factory::SetupX509() {

    auto env = XrdCl::DefaultEnv::GetEnv();
    std::string cert_file;
    env->ResolveString("HttpCertFile", "XRD_HTTPCERTFILE", &m_plugin_config, cert_file, "");
    LogIfSet(*m_log, "HttpCertFile", cert_file);
    std::string cert_dir;
    env->ResolveString("HttpCertDir", "XRD_HTTPCERTDIR", &m_plugin_config, cert_dir, "");
    LogIfSet(*m_log, "HttpCertDir", cert_dir);
    std::string client_cert_file;
    env->ResolveString("HttpClientCertFile", "XRD_HTTPCLIENTCERTFILE", &m_plugin_config, client_cert_file, "");
    LogIfSet(*m_log, "HttpClientCertFile", client_cert_file);
    std::string client_key_file;
    env->ResolveString("HttpClientKeyFile", "XRD_HTTPCLIENTKEYFILE", &m_plugin_config, client_key_file, "");
    LogIfSet(*m_log, "HttpClientKeyFile", client_key_file);

    int disable_proxy = 0;
    env->ResolveInt("HttpDisableX509", "XRD_HTTPDISABLEX509", &m_plugin_config, disable_proxy, 0);

    std::string filename;
    char *filename_char;
    if (!disable_proxy && (!env->GetString("HttpClientCertFile", filename) || filename.empty())) {
        if ((filename_char = getenv("X509_USER_PROXY"))) {
            filename = filename_char;
        }
        if (filename.empty()) {
            filename = "/tmp/x509up_u" + std::to_string(geteuid());
        }
        if (access(filename.c_str(), R_OK) == 0) {
            m_log->Debug(kLogXrdClHttp, "Using X509 proxy file found at %s for TLS client credential", filename.c_str());
            env->PutString("HttpClientCertFile", filename);
            env->PutString("HttpClientKeyFile", filename);
        }
    }
    if ((!env->GetString("HttpCertDir", filename) || filename.empty()) && (filename_char = getenv("X509_CERT_DIR"))) {
        env->PutString("HttpCertDir", filename_char);
    }
}

void
Factory::Shutdown()
{
    {
        std::unique_lock lock(m_shutdown_lock);
        m_shutdown_requested = true;
        m_shutdown_requested_cv.notify_one();
    }
    if (m_monitor_tid.joinable()) {
      m_monitor_tid.join();
    }
}

void
Factory::Produce(std::unique_ptr<XrdClHttp::CurlOperation> operation)
{
    m_queue->Produce(std::move(operation));
}

XrdCl::FilePlugIn *
Factory::CreateFile(const std::string & /*url*/) {
    Initialize();
    if (!m_initialized) {return nullptr;}
    return new File(m_queue, m_log);
}

XrdCl::FileSystemPlugIn *
Factory::CreateFileSystem(const std::string & url) {
    Initialize();
    if (!m_initialized) {return nullptr;}
    return new Filesystem(url, m_queue, m_log);
}

extern "C"
{
    void *XrdClGetPlugIn(const void *arg)
    {
        if (arg) {
            auto config = static_cast<const std::map<std::string, std::string> *>(arg);
            Factory::SetPluginConfig(*config);
        }
        return static_cast<void*>(new Factory());
    }
}

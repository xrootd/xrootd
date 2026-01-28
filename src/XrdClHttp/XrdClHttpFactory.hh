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

#ifndef XRDCLHTTP_FACTORY_HH
#define XRDCLHTTP_FACTORY_HH

#include "XrdCl/XrdClPlugInInterface.hh"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <time.h>
#include <thread>
#include <vector>

namespace XrdCl {
    class Log;
}

namespace XrdClHttp {

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
    void Produce(std::unique_ptr<XrdClHttp::CurlOperation> operation);

private:
    // Actual initialization of the factory.  Only done when the first filesystem/file
    // is created to allow a parent process to fork first.
    void Initialize();

    // Set the various X509 credential variables in the default environment.
    void SetupX509();

    // Monitoring loop for XrdClHttp statistics
    void Monitor();

    // Invoked by the destructor of a static member, to know when the
    // the library is shutting down or is unloaded from the process.
    static void Shutdown();

    static bool m_initialized;
    static std::shared_ptr<XrdClHttp::HandlerQueue> m_queue;
    static XrdCl::Log *m_log;
    const static unsigned m_poll_threads{8};
    static std::once_flag m_init_once;
    // Location for the client to dump its runtime statistics.
    static std::string m_stats_location;

    // Start time of the factory
    static std::chrono::system_clock::time_point m_start;

    // Mutex for managing the shutdown of the background thread
    static std::mutex m_shutdown_lock;
    // The background thread
    static std::thread m_monitor_tid;
    // Condition variable managing the requested shutdown of the background thread.
    static std::condition_variable m_shutdown_requested_cv;
    // Flag indicating that a shutdown was requested.
    static bool m_shutdown_requested;
    // shutdown trigger
    static struct shutdown_s {
      ~shutdown_s() { Shutdown(); }
    } m_shutdowns;
};

}

#endif // XRDCLHTTP_FACTORY_HH

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

#include "XrdClCurlOptionsCache.hh"

#include <curl/curl.h>

#include <thread>

XrdClCurl::VerbsCache XrdClCurl::VerbsCache::g_cache;
std::once_flag XrdClCurl::VerbsCache::m_expiry_launch;
std::mutex XrdClCurl::VerbsCache::m_shutdown_lock;
std::condition_variable XrdClCurl::VerbsCache::m_shutdown_requested_cv;
bool XrdClCurl::VerbsCache::m_shutdown_requested = false;
std::condition_variable XrdClCurl::VerbsCache::m_shutdown_complete_cv;
bool XrdClCurl::VerbsCache::m_shutdown_complete = true; // Starts in "true" state as the thread hasn't started

XrdClCurl::VerbsCache & XrdClCurl::VerbsCache::Instance() {
    std::call_once(m_expiry_launch, [] {
        {
            std::unique_lock lk(m_shutdown_lock);
            m_shutdown_complete = false;
        }
        std::thread t(VerbsCache::ExpireThread);
        t.detach();
    });
    return g_cache;
}

void XrdClCurl::VerbsCache::ExpireThread()
{
    while (true) {
        {
            std::unique_lock lock(m_shutdown_lock);
            m_shutdown_requested_cv.wait_for(
                lock,
                std::chrono::seconds(30),
                []{return m_shutdown_requested;}
            );
            if (m_shutdown_requested) {
                break;
            }
        }
        auto now = std::chrono::steady_clock::now();
        g_cache.Expire(now);
    }
    std::unique_lock lock(m_shutdown_lock);
    m_shutdown_complete = true;
    m_shutdown_complete_cv.notify_one();
}

void XrdClCurl::VerbsCache::Expire(std::chrono::steady_clock::time_point now)
{
    std::unique_lock lock(m_mutex);
    for (auto iter = m_verbs_map.begin(); iter != m_verbs_map.end();) {
        if (iter->second.m_expiry < now) {
            iter = m_verbs_map.erase(iter);
        } else {
            ++iter;
        }
    }
}

void
XrdClCurl::VerbsCache::Shutdown()
{
    std::unique_lock lock(m_shutdown_lock);
    m_shutdown_requested = true;
    m_shutdown_requested_cv.notify_one();

    m_shutdown_complete_cv.wait(lock, []{return m_shutdown_complete;});
}

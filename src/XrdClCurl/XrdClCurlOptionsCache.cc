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

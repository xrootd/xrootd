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

#ifndef _XRDCLHTTP__OPTIONSCACHE_HH__
#define _XRDCLHTTP__OPTIONSCACHE_HH__

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
 
 namespace XrdClHttp {
 
 // A cache holding the known HTTP verbs for a given endpoint.
 class VerbsCache {
 public:
 
    // Enumeration bitmask of the HTTP verbs that we can test for
    enum class HttpVerb {
        kUnset = 0, // Indicates that we haven't yet probed for the HTTP verb support.
        kUnknown = 1, // Indicates we probed for support but the result was indeterminate (not provided by the server, network error)
        kPROPFIND = 2, // Server claims to support PROPFIND
    };
    class HttpVerbs {
    public:
        HttpVerbs() = default;
        HttpVerbs(HttpVerb verb) : m_verbs(static_cast<unsigned>(verb)) {}
        HttpVerbs &operator|=(HttpVerb verb) {m_verbs |= static_cast<unsigned>(verb); return *this;}
        bool IsSet(HttpVerb verb) const
        {
            if (verb == HttpVerb::kUnset) {return !m_verbs;}
            return static_cast<unsigned>(m_verbs) & static_cast<unsigned>(verb);
        }
        unsigned GetValue() const {return m_verbs;}
    private:
        unsigned m_verbs{0};
    };
 
    static const std::string GetVerbString(HttpVerb ctype) {
        switch (ctype) {
            case HttpVerb::kUnset:
                return "(unset)";
            case HttpVerb::kUnknown:
                return "(unknown)";
            case HttpVerb::kPROPFIND:
                return "PROPFIND";
        }
    }

    void Put(const std::string &url, const HttpVerbs &verbs, const std::chrono::steady_clock::time_point &now=std::chrono::steady_clock::now()) const {
        auto key = GetUrlKey(url);
        if (key.empty()) return;

        const std::unique_lock sentry(m_mutex);

        auto isKnown = !verbs.IsSet(HttpVerb::kUnknown);
        auto lifetime = isKnown ? g_expiry_duration : g_negative_expiry_duration;

        auto iter = m_verbs_map.find(key);
        if (iter == m_verbs_map.end()) {
            m_verbs_map.emplace(std::move(key),
                                VerbEntry{now + lifetime, verbs});
        } else if (isKnown || iter->second.m_verbs.IsSet(HttpVerb::kUnknown)) {
            // Previous entry didn't know the verbs, but now we do
            iter->second = {now + lifetime, verbs};
        }
    }

    HttpVerbs Get(const std::string &url, const std::chrono::steady_clock::time_point &now=std::chrono::steady_clock::now()) const {
        auto key = GetUrlKey(url);
        if (key.empty()) {
            m_cache_miss++;
            return HttpVerbs{};
        }

        const std::shared_lock sentry(m_mutex);
        auto iter = m_verbs_map.find(key);
        if (iter == m_verbs_map.end()) {
            m_cache_miss++;
            return HttpVerbs{};
        }
        if (iter->second.m_expiry < now) {
            m_cache_miss++;
            return HttpVerbs{};
        }
        m_cache_hit++;
        return iter->second.m_verbs;
    }

    // Return a normalized resource URL without credentials or request parameters.
    static std::string GetUrlKey(const std::string &url);

    uint64_t GetCacheHits() const {return m_cache_hit;}
    uint64_t GetCacheMisses() const {return m_cache_miss;}

    // Expire all entries in the cache whose expiration is older than `now`.
    void Expire(std::chrono::steady_clock::time_point now);

    // Return the global instance of the verbs cache.
    static VerbsCache &Instance();

private:
    VerbsCache() = default;
    VerbsCache(const VerbsCache &) = delete;
    VerbsCache(VerbsCache &&) = delete;

    // Background thread periodically invoking `Expire` on the cache.
    static void ExpireThread();

    // Invoked by the destructor of a static member. Triggered when the library
    // is shutting down or is unloaded from the process.
    static void Shutdown();

    mutable std::atomic<uint64_t> m_cache_hit{0};
    mutable std::atomic<uint64_t> m_cache_miss{0};

    struct VerbEntry {
        std::chrono::steady_clock::time_point m_expiry;
        HttpVerbs m_verbs;
    };

    mutable std::shared_mutex m_mutex;
    mutable std::unordered_map<std::string, VerbEntry> m_verbs_map;

    static std::once_flag m_expiry_launch;
    static VerbsCache g_cache;
    static constexpr std::chrono::steady_clock::duration g_expiry_duration = std::chrono::hours(6);
    static constexpr std::chrono::steady_clock::duration g_negative_expiry_duration = std::chrono::minutes(15);

    // Mutex for managing the shutdown of the background thread
    static std::mutex m_shutdown_lock;
    // Condition variable managing the requested shutdown of the background thread.
    static std::condition_variable m_shutdown_requested_cv;
    // Flag indicating that a shutdown was requested.
    static bool m_shutdown_requested;
    // The cache expire thread
    static std::thread m_expire_tid;
    // shutdown trigger
    static struct shutdown_s {
      ~shutdown_s() { Shutdown(); }
    } m_shutdowns;
};
 
 } // namespace XrdClHttp
 
#endif // _XRDCLHTTP__OPTIONSCACHE_HH__

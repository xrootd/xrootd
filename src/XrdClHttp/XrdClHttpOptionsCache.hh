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
        std::string modified_url;
        auto key = GetUrlKey(url, modified_url);

        const std::unique_lock sentry(m_mutex);

        auto isKnown = !verbs.IsSet(HttpVerb::kUnknown);
        auto lifetime = isKnown ? g_expiry_duration : g_negative_expiry_duration;

// C++20 can elide the allocation for the string_view
#if __cplusplus >= 202002L
        auto iter = m_verbs_map.find(key);
#else
        auto iter = m_verbs_map.find(std::string(key));
#endif
        if (iter == m_verbs_map.end()) {
            m_verbs_map.emplace(key, VerbEntry{now + lifetime, verbs});
        } else if (isKnown || iter->second.m_verbs.IsSet(HttpVerb::kUnknown)) {
            // Previous entry didn't know the verbs, but now we do
            iter->second = {now + lifetime, verbs};
        }
    }

    HttpVerbs Get(const std::string &url, const std::chrono::steady_clock::time_point &now=std::chrono::steady_clock::now()) const {
        std::string modified_url;
        auto key = GetUrlKey(url, modified_url);

        const std::shared_lock sentry(m_mutex);
#if __cplusplus >= 202002L
        auto iter = m_verbs_map.find(key);
#else
        auto iter = m_verbs_map.find(std::string(key));
#endif
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

    // Get the cache key for a given URL
    //
    // Cache key should consist of the schema, host, and port portion of the URL.
    static std::string_view GetUrlKey(const std::string &url, std::string &modified_url) {
        auto authority_loc = url.find("://");
        if (authority_loc == std::string::npos) {
            return std::string_view();
        }
        auto path_loc = url.find('/', authority_loc + 3);
        if (path_loc == std::string::npos) {
            path_loc = url.length();
        }

        std::string_view url_view{url};
        auto host_loc = url_view.substr(authority_loc + 3, path_loc - authority_loc - 3).find('@');
        if (host_loc == std::string::npos) {
            return url_view.substr(0, path_loc);
        }
        host_loc += authority_loc + 3;
        modified_url = url.substr(0, authority_loc + 3) + std::string(url_view.substr(host_loc + 1, path_loc - host_loc - 1));
        return modified_url;
    }

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

    // Invoked by libc when the library is shutting down or is unloaded from the process.
    static void Shutdown() __attribute__((destructor));

    mutable std::atomic<uint64_t> m_cache_hit{0};
    mutable std::atomic<uint64_t> m_cache_miss{0};

    template<typename ... Bases>
    struct overload : Bases ...
    {
        using is_transparent = void;
        using Bases::operator() ... ;
    };
    using transparent_string_hash = overload<
        std::hash<std::string>,
        std::hash<std::string_view>
    >;

    struct VerbEntry {
        std::chrono::steady_clock::time_point m_expiry;
        HttpVerbs m_verbs;
    };

    mutable std::shared_mutex m_mutex;
    mutable std::unordered_map<std::string, VerbEntry, VerbsCache::transparent_string_hash, std::equal_to<>> m_verbs_map;

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
    // Condition variable for the background thread to indicate it has completed.
    static std::condition_variable m_shutdown_complete_cv;
    // Flag indicating that the shutdown has completed.
    static bool m_shutdown_complete;
};
 
 } // namespace XrdClHttp
 
#endif // _XRDCLHTTP__OPTIONSCACHE_HH__

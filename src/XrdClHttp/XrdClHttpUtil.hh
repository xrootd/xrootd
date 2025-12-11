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

#ifndef XRDCLHTTPUTIL_HH
#define XRDCLHTTPUTIL_HH

#include "XrdClHttpChecksum.hh"
#include "XrdClHttpOptionsCache.hh"
#include "XrdClHttpResponseInfo.hh"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward dec'ls
typedef void CURL;
struct curl_slist;

namespace XrdCl {

class ResponseHandler;
class Log;

}

namespace XrdClHttp {

class CurlOperation;

const uint64_t kLogXrdClHttp = 73173;

bool HTTPStatusIsError(unsigned status);

std::pair<uint16_t, uint32_t> HTTPStatusConvert(unsigned status);

// Trim the left side of a string_view for space
std::string_view ltrim_view(const std::string_view &input_view);

// Trim the left and right side of a string_view of whitespace
std::string_view trim_view(const std::string_view &input_view);

// Returns a newly-created curl handle (no internal caching) with the
// various configurations needed to be used by XrdClHttp
CURL *GetHandle(bool verbose);

// Parser for headers as emitted by libcurl.
//
// Records specific headers known to be used by the project but ignores others.
class HeaderParser {
public:
    HeaderParser() {}

    bool Parse(const std::string &headers);

    int64_t GetContentLength() const {return m_content_length;}

    uint64_t GetOffset() const {return m_response_offset;}

    static bool Canonicalize(std::string &headerName);

    bool HeadersDone() const {return m_recv_all_headers;}

    // Move the received headers to the caller.
    //
    // Only invoke once HeadersDone() returns true.
    ResponseInfo::HeaderMap && MoveHeaders() {return std::move(m_headers);}

    int GetStatusCode() const {return m_status_code;}

    // Setter for the status code
    // Intended for use in unit tests.
    void SetStatusCode(int sc) {m_status_code = sc;}

    // Return whether the server response specified this is a multipart range.
    bool IsMultipartByterange() const {return m_multipart_byteranges;}

    // Return the separator specified in the server response with the
    // `--` prefix included..
    const std::string &MultipartSeparator() const {return m_multipart_sep;}

    // Set the separator used for multipart messages; a `--` prefix
    // will be added to the Getter.
    void SetMultipartSeparator(const std::string_view &sep) {
        m_multipart_sep = "--" + std::string(sep);
        m_multipart_byteranges = true;
    }

    VerbsCache::HttpVerbs GetAllowedVerbs() const
    {
        return VerbsCache::HttpVerbs(m_allow_verbs);
    }

    std::string GetStatusMessage() const {return m_resp_message;}

    const std::string &GetLocation() const {return m_location;}
    const std::string &GetETag() const {return m_etag;}
    const std::string &GetCacheControl() const {return m_cache_control;}

    // Returns a reference to the checksums parsed from the headers.
    const XrdClHttp::ChecksumInfo &GetChecksums() const {return m_checksums;}

    // Parse a RFC 3230 header, updating the checksum info structure.
    static void ParseDigest(const std::string &digest, XrdClHttp::ChecksumInfo &info);

    // Decode a base64-encoded string into a binary buffer.
    static bool Base64Decode(std::string_view input, std::array<unsigned char, 32> &output);

    // Convert a checksum type to a RFC 3230 digest name.
    static std::string ChecksumTypeToDigestName(XrdClHttp::ChecksumType type);

private:

    static bool validHeaderByte(unsigned char c);

    int64_t m_content_length{-1};
    uint64_t m_response_offset{0};

    XrdClHttp::ChecksumInfo m_checksums;

    bool m_recv_all_headers{false};
    bool m_recv_status_line{false};
    bool m_multipart_byteranges{false};

    int m_status_code{-1};
    std::string m_resp_protocol;
    std::string m_resp_message;
    std::string m_location;
    std::string m_multipart_sep;
    std::string m_etag;
    std::string m_cache_control;

    ResponseInfo::HeaderMap m_headers;

    VerbsCache::HttpVerb m_allow_verbs{VerbsCache::HttpVerb::kUnknown};
};

/**
 * HandlerQueue is a deque of curl operations that need
 * to be performed.  The object is thread safe and can
 * be waited on via poll().
 *
 * The fact that it's poll'able is necessary because the
 * multi-curl driver thread is based on polling FD's
 */
class HandlerQueue {
public:
    HandlerQueue(unsigned max_pending_ops);

    void Produce(std::shared_ptr<CurlOperation> handler);

    std::shared_ptr<CurlOperation> Consume(std::chrono::steady_clock::duration);
    std::shared_ptr<CurlOperation> TryConsume();

    int PollFD() const {return m_read_fd;}

    CURL *GetHandle();
    void RecycleHandle(CURL *);

    // Check all the operations in queue to see if any have expired.
    //
    // Each curl operation has a header timeout; if no headers have been received
    // by the time the timeout expires, the operation is considered to have
    // expired.  This function checks all operations in the queue and
    // removes any that have expired.
    void Expire();

    void Shutdown();
    // Cleanup all idle handles in current thread.
    void ReleaseHandles();

    // Returns the class default number of pending operations.
    static unsigned GetDefaultMaxPendingOps() {return m_default_max_pending_ops;}

    // Returns a summary of the queue's performance statistics.
    static std::string GetMonitoringJson();

private:
    bool m_shutdown{false};
    std::deque<std::shared_ptr<CurlOperation>> m_ops;
    static std::atomic<uint64_t> m_ops_consumed; // Count of operations consumed from the queue.
    static std::atomic<uint64_t> m_ops_produced; // Count of operations added to the queue.
    static std::atomic<uint64_t> m_ops_rejected; // Count of operations rejected by the queue.
    thread_local static std::vector<CURL*> m_handles;
    std::condition_variable m_consumer_cv;
    std::condition_variable m_producer_cv;
    std::mutex m_mutex;
    const static unsigned m_default_max_pending_ops{50};
    const unsigned m_max_pending_ops{50};
    int m_read_fd{-1};
    int m_write_fd{-1};
};

}

#endif // XRDCLHTTPUTIL_HH

/***************************************************************
 *
 * Copyright (C) 2025, Morgridge Institute for Research
 *
 ***************************************************************/

#ifndef XRDCLHTTP_CURLRESPONSEINFO_HH
#define XRDCLHTTP_CURLRESPONSEINFO_HH

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace XrdClHttp {

// Representation of the information contained in a response.
class ResponseInfo {
public:
    // Force the response information to be virtual, allowing RTTI and easing potential ABI changes
    virtual ~ResponseInfo() {};

    // A list of header values in the order they occurred in the response.
    // Typically, only one element is in the vector.
    using HeaderValues = std::vector<std::string>;

    // Map of header key to header value(s).  The header key is *case sensitive*;
    // queries should be done using the "canonical" form (see
    // https://cs.opensource.google/go/go/+/refs/tags/go1.24.2:src/net/textproto/reader.go;l=647)
    using HeaderMap = std::unordered_map<std::string, HeaderValues>;

    // List of response headers, one entry per response received through the
    // operation; potentially there are multiple if there was a redirect
    using HeaderResponses = std::vector<HeaderMap>;

    const HeaderResponses &GetHeaderResponse() const {return m_header_responses;}

    // Adds a set of headers to the internal response object.
    void AddResponse(HeaderMap &&map) {m_header_responses.emplace_back(map);}

private:
    std::vector<std::unordered_map<std::string, std::vector<std::string>>> m_header_responses;
};

} // namespace XrdClHttp

#endif // XRDCLHTTP_CURLRESPONSEINFO_HH

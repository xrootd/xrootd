/***************************************************************
 *
 * Copyright (C) 2025, Morgridge Institute for Research
 *
 ***************************************************************/

#ifndef XRDCLHTTP_CURLHEADER_CALLOUT_HH
#define XRDCLHTTP_CURLHEADER_CALLOUT_HH

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace XrdClHttp {

class HeaderCallout {
public:
    // A list of headers, represented as pairs of strings (name, value).
    using HeaderList = std::vector<std::pair<std::string, std::string>>;

    virtual ~HeaderCallout() noexcept = default;

    // Return a header list based on the HTTP `verb`, `url`, and existing headers.
    virtual std::shared_ptr<HeaderList> GetHeaders(const std::string &verb, const std::string &url, const HeaderList &headers) = 0;
};

}

#endif // XRDCLHTTP_CURLHEADER_CALLOUT_HH
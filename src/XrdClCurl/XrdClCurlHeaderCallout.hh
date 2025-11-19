/***************************************************************
 *
 * Copyright (C) 2025, Morgridge Institute for Research
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

#ifndef XRDCLCURL_CURLHEADER_CALLOUT_HH
#define XRDCLCURL_CURLHEADER_CALLOUT_HH

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace XrdClCurl {

class HeaderCallout {
public:
    // A list of headers, represented as pairs of strings (name, value).
    using HeaderList = std::vector<std::pair<std::string, std::string>>;

    virtual ~HeaderCallout() noexcept = default;

    // Return a header list based on the HTTP `verb`, `url`, and existing headers.
    virtual std::shared_ptr<HeaderList> GetHeaders(const std::string &verb, const std::string &url, const HeaderList &headers) = 0;
};

}

#endif // XRDCLCURL_CURLHEADER_CALLOUT_HH
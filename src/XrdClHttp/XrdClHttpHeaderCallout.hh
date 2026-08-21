/******************************************************************************/
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
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
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

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
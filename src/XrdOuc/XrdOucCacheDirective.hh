/******************************************************************************/
/*                                                                            */
/*              X r d O u c C a c h e D i r e c t i v e . h h                 */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#pragma once

#include <string>
#include <vector>

/**
 * Helper class for parsing the known cache headers.
 *
 * Purposely hews to the HTTP cache-control headers where possible.  See
 *   <https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control>
 * for a detailed explanation of the different directives.
 */
class XrdOucCacheDirective {

public:
    /**
     * Given a header value, parse out to a list of known cache directives
     */
    XrdOucCacheDirective(const std::string &header);

    /**
     * Returns a list of unknown directives provided.
     */
    const std::vector<std::string> UnknownDirectives() const;

    /**
     * Returns true if the `no-cache` directive is present.
     */
    bool NoCache() const {return m_no_cache;}

    /**
     * Returns true if the `no-store` directive is present.
     */
    bool NoStore() const {return m_no_store;}

    /**
     * Returns true if the `only-if-cached` directive is present.
     */
    bool OnlyIfCached() const {return m_only_if_cached;}

    /**
     * Returns the value of the `max-age` directive; if the directive
     * is not present, returns -1.
     */
    int MaxAge() const {return m_max_age;}

private:
    bool m_no_cache{false};
    bool m_no_store{false};
    bool m_only_if_cached{false};
    int m_max_age{-1};

    std::vector<std::string> m_unknown;
};

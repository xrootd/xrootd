/******************************************************************************/
/*                                                                            */
/*              X r d O u c C a c h e D i r e c t i v e . c c                 */
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

#include "XrdOuc/XrdOucCacheDirective.hh"
#include "XrdOuc/XrdOucString.hh"

#include <cstring>

XrdOucCacheDirective::XrdOucCacheDirective(const std::string &header)
{
    XrdOucString header_ouc(header.c_str());
    int from = 0;
    XrdOucString directive;
    while ((from = header_ouc.tokenize(directive, from, ',')) != -1) {

        // Trim out whitespace, make lowercase
        int begin = 0;
        while (begin < directive.length() && !isgraph(directive[begin])) {begin++;}
        if (begin) directive.erasefromstart(begin);
        if (directive.length()) {
            int endtrim = 0;
            while (endtrim < directive.length() && !isgraph(directive[directive.length() - endtrim - 1])) {endtrim++;}
            if (endtrim) directive.erasefromend(endtrim);
        }
        if (directive.length() == 0) {continue;}
        directive.lower(0);

        int pos = directive.find('=');
        // No known cache directive command is larger than 19 characters;
        // use this fact so we can have a statically sized buffer.
        if (pos > 19 || directive.length() > 19) {
            m_unknown.push_back(directive.c_str());
            continue;
        }
        char command[20];
        command[19] = '\0';
        if (pos == STR_NPOS) {
            strncpy(command, directive.c_str(), 19);
        } else {
            memcpy(command, directive.c_str(), pos);
            command[pos] = '\0';
        }
        if (!strcmp(command, "no-cache")) {
            m_no_cache = true;
        } else if (!strcmp(command, "no-store")) {
            m_no_store = true;
        } else if (!strcmp(command, "only-if-cached")) {
            m_only_if_cached = true;
        } else if (!strcmp(command, "max-age")) {
            std::string value(directive.c_str() + pos + 1);
            try {
                m_max_age = std::stoi(value);
            } catch (...) {
                m_unknown.push_back(directive.c_str());
            }
        } else {
            m_unknown.push_back(directive.c_str());
        }
    }
}

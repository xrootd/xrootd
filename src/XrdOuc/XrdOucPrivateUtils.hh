//------------------------------------------------------------------------------
// Copyright (c) 2024-2025 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <cedric.caffy@cern.ch>
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------


/**
 * PRIVATE HEADER for utility functions, implementation in XrdOucUtils.cc
 */
#ifndef XROOTD_XRDOUCPRIVATEUTILS_HH
#define XROOTD_XRDOUCPRIVATEUTILS_HH

#include "XrdOuc/XrdOucString.hh"

#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * Returns true if path @p subdir is a subdirectory of @p dir.
 */
static inline bool is_subdirectory(const std::string& dir,
                                   const std::string& subdir)
{
    if (subdir.size() < dir.size() || dir.empty())
      return false;

    if (subdir.compare(0, dir.size(), dir, 0, dir.size()) != 0)
      return false;

    return dir.size() == subdir.size() || subdir[dir.size()] == '/' || dir.back() == '/';
}

/**
 * Obfuscates strings containing "authz=value", "Authorization: value",
 * "TransferHeaderAuthorization: value", "WhateverAuthorization: value"
 * in a case insensitive way.
 *
 * @param input the string to obfuscate
 */
std::string obfuscateAuth(const std::string & input);

/**
 * Strip selected CGI elements (e.g. "authz=...") from a string/URL.
 * The function removes occurrences of "<key>=<token>" for each key in cgiKeys
 *
 * @param url the string/URL to sanitize (modified in-place)
 * @param cgiKeys CGI parameter names to remove (without the trailing '=')
 */
void stripCgi(std::string& url, const std::unordered_set<std::string> &cgiKeys);
void stripCgi(XrdOucString& url, const std::unordered_set<std::string> &cgiKeys);

#endif //XROOTD_XRDOUCPRIVATEUTILS_HH

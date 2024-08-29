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

#include <string>
#include <vector>
#include <regex>

extern const std::string OBFUSCATION_STR;

// As the compilation of the regexes when the std::regex object is constructed is expensive,
// we initialize the auth obfuscation regexes only once in the XRootD process lifetime
extern const std::vector<std::regex> authObfuscationRegexes;

/**
 * Obfuscates string containing authz=value key value and Authorization: value, TransferHeaderAuthorization: value, WhateverAuthorization: value
 * in a case insensitive way
 * @param input the string to obfuscate
 */
std::string obfuscateAuth(const std::string & input);

/**
 * Use this function to obfuscate any string containing key-values with OBFUSCATION_STR
 * @param input the string to obfuscate
 * @param regexes the obfuscation regexes to apply to replace the value with OBFUSCATION_STR. The key should be a regex group e.g: "(authz=)"
 * Have a look at obfuscateAuth for more examples
 * @return the string with values obfuscated
 */
std::string obfuscate(const std::string &input, const std::vector<std::regex> &regexes);

#endif //XROOTD_XRDOUCPRIVATEUTILS_HH

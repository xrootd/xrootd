//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2025 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <ccaffy@cern.ch>
// File Date: Jun 2025
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
#include "XrdHttpHeaderUtils.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include <vector>
#include <algorithm>


void XrdHttpHeaderUtils::parseReprDigest(const std::string &header, std::map<std::string, std::string> &output) {
  // Expected format per entry: <cksumType>=:<digestValue>:
  std::vector<std::string> digestNameValuePairs;
  XrdOucTUtils::splitString(digestNameValuePairs, header, ",");

  for (const auto &digestNameValue : digestNameValuePairs) {
    auto equalPos = digestNameValue.find('=');
    if (equalPos == std::string::npos || equalPos >= digestNameValue.size() - 1)
      continue;

    std::string cksumType = digestNameValue.substr(0, equalPos);
    XrdOucUtils::trim(cksumType);
    if (cksumType.empty())
      continue;

    std::string cksumValueIn = digestNameValue.substr(equalPos + 1);
    size_t beginCksumPos = cksumValueIn.find(':');
    size_t endCksumPos = cksumValueIn.rfind(':');

    // Check that the string starts with ':' and contains two distinct colons
    if (beginCksumPos == 0 && endCksumPos > beginCksumPos + 1 && endCksumPos < cksumValueIn.size()) {
      std::string cksumValue = cksumValueIn.substr(beginCksumPos + 1, endCksumPos - beginCksumPos - 1);
      XrdOucUtils::trim(cksumValue);
      if (!cksumValue.empty())
        output[cksumType] = cksumValue;
    }
    // Malformed entries are silently ignored
  }
}
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
#include "XrdHttpUtils.hh"

#include <vector>
#include <algorithm>


void XrdHttpHeaderUtils::parseReprDigest(const std::string &value, std::map<std::string, std::string> &output) {
  // Expected format per entry: <cksumType>=:<digestValue>:
  std::vector<std::string> digestNameValuePairs;
  XrdOucTUtils::splitString(digestNameValuePairs, value, ",");

  for (const auto &digestNameValue : digestNameValuePairs) {
    std::string_view digestNameValueSV {digestNameValue};
    auto equalPos = digestNameValueSV.find('=');
    if (equalPos == std::string_view::npos || equalPos >= digestNameValueSV.size() - 1)
      continue;

    std::string_view cksumTypeSV = digestNameValueSV.substr(0, equalPos);
    XrdOucUtils::trim(cksumTypeSV);
    if (cksumTypeSV.empty())
      continue;

    std::string_view cksumValueInSV = digestNameValueSV.substr(equalPos + 1);
    size_t beginCksumPos = cksumValueInSV.find(':');
    size_t endCksumPos = cksumValueInSV.rfind(':');

    // Check that the string starts with ':' and contains two distinct colons
    if (beginCksumPos == 0 && endCksumPos > beginCksumPos + 1 && endCksumPos < cksumValueInSV.size()) {
      std::string_view cksumValue = cksumValueInSV.substr(beginCksumPos + 1, endCksumPos - beginCksumPos - 1);
      XrdOucUtils::trim(cksumValue);
      if (!cksumValue.empty()) {
        //What we get as checksum value is a base64-encoded hexadecimal bytes
        //Let's decode that.
        std::string chksumDecoded;
        base64DecodeHex(std::string(cksumValue), chksumDecoded);
        std::string cksumTypeLC {cksumTypeSV};
        std::transform(cksumTypeLC.begin(), cksumTypeLC.end(), cksumTypeLC.begin(), ::tolower);
        output[cksumTypeLC] = chksumDecoded;
      }
    }
    // Malformed entries are silently ignored
  }
}

void XrdHttpHeaderUtils::parseWantReprDigest(const std::string & value, std::map<std::string, uint8_t> &output) {
  size_t pos = 0;
  std::string_view value_sv {value};
  while(pos <= value_sv.size()) {
    // find comma
    size_t comma = value.find(',',pos);
    // extract item, no comma means the item is the full string
    std::string_view item = (comma == std::string_view::npos) ? value_sv.substr(pos) : value_sv.substr(pos, comma - pos);
    // move current cursor to 'comma + 1' or after the string end
    pos = (comma == std::string_view::npos) ? value.size() + 1 : comma + 1;
    // trim the item
    XrdOucUtils::trim(item);
    if(item.empty()) continue;

    size_t eq = item.find('=');
    // If no '=' sign, we discard this entry as it is malformed
    if(eq == std::string_view::npos) continue;
    // We found the equal sign on the item
    std::string_view digestName {item.substr(0, eq)};
    XrdOucUtils::trim(digestName);
    std::string_view preference {item.substr(eq+1)};
    XrdOucUtils::trim(preference);

    std::string key_lower {digestName};
    std::transform(key_lower.begin(),key_lower.end(),key_lower.begin(),::tolower);

    try {
      uint8_t preference_us = XrdOucUtils::touint8_t(preference);
      // Max allowed value for Repr-Digest is 10
      preference_us = std::min(preference_us,(uint8_t)10);
      output[key_lower] = preference_us;
    } catch (...) {
      // discard invalid values
    }
  }
}
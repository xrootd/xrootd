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

#ifndef XROOTD_XRDHTTPHEADERUTILS_HH
#define XROOTD_XRDHTTPHEADERUTILS_HH

#include <map>
#include <string>
#include <cstdint>

class XrdHttpHeaderUtils {
public:

  /**
   * Parses the 'Repr-Digest' header value received from the client
   * Syntax: "Repr-Digest: adler=:base64EncodedValue:, crc32=:base64EncodedValue:
   * @param value contains the value of the header Repr-Digest
   * @param output the map containing the digests and their associated base64 encoded values
   */
  static void parseReprDigest(const std::string & value, std::map<std::string,std::string> & output);

  /**
   * Parses 'Want-Repr-Digest' header value received from the client
   * Syntax: "Want-Repr-Digest: adler=1, crc32=2, sha-256=9
   * The values are integers representing the preference, comprised between 0 and 9.
   *
   * @param value contains the value of the header Want-Repr-Digest
   * @param output the map containing the lower-cased digest name and the associated preference
   */
  static void parseWantReprDigest(const std::string & value, std::map<std::string, uint8_t> & output);
};


#endif //XROOTD_XRDHTTPHEADERUTILS_HH

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
#include <sys/types.h>

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

  /**
   * Parses the value of a 'Content-Length' HTTP header.
   *
   * The HTTP spec says Content-Length must be one or more decimal digits
   * with no sign character and no whitespace inside the number. Anything
   * else is malformed and must be rejected with HTTP 400, because if we
   * trust a garbage Content-Length we may disagree with a proxy in front
   * of us on where the request body ends — which is the basis of HTTP
   * request smuggling attacks.
   *
   * Trailing whitespace and CRLF in the value are tolerated (parseLine
   * forwards the raw header line including the closing \r\n).
   *
   * Reference: RFC 9112 §6.2 (Content-Length syntax) and RFC 7230 §3.3.3
   * rule 4 (rejection of invalid values).
   *
   * @param value the raw value of the Content-Length header
   * @return the parsed non-negative integer on success, or a distinct
   *         negative code on error:
   *           -1 the value is empty (after trimming trailing CRLF / OWS),
   *           -2 the value contains a non-digit character (sign, garbage,
   *              embedded whitespace),
   *           -3 the value is numerically too large for an ssize_t.
   */
  static ssize_t parseContentLength(const std::string & value);

  /**
   * Parses the value of a 'Transfer-Encoding' HTTP header.
   *
   * Transfer-Encoding carries a comma-separated list of body encodings,
   * for example "chunked" or "gzip, chunked". The list is case-insensitive
   * and items can have whitespace around them.
   *
   * The only encoding this server speaks is "chunked". HTTP requires
   * that whenever "chunked" appears it must be the LAST item in the list
   * (because that is the one that determines how the body is framed
   * on the wire).
   *
   * This function checks two things together: that "chunked" is actually
   * present (matched case-insensitively as a whole token, not as a
   * substring like the old code did), and that it is the last item.
   *
   * Trailing CRLF on the last item is tolerated.
   *
   * Reference: RFC 9112 §6.1 (Transfer-Encoding syntax and the
   * "chunked-must-be-last" rule) and RFC 7230 §3.3.1 (same rule).
   *
   * @param value the raw value of the Transfer-Encoding header
   * @return 0 on success (the list ends in "chunked"), or a distinct
   *         negative code on error:
   *           -1 the value is empty (no tokens after splitting/trimming),
   *           -2 there is no "chunked" token in the list at all,
   *           -3 "chunked" appears but is not the final token.
   */
  static int parseTransferEncoding(const std::string & value);
};


#endif //XROOTD_XRDHTTPHEADERUTILS_HH

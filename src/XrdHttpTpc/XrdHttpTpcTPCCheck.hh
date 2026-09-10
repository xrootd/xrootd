//------------------------------------------------------------------------------
// This file is part of XrdHttpTpcTPC
//
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <ccaffy@cern.ch>
// File Date: Sep 2026
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

#ifndef XROOTD_XRDHTTPTPCTPCCHECK_HH
#define XROOTD_XRDHTTPTPCTPCCHECK_HH

#include <string>

/**
 * Validation of a COPY request, done before anything is opened.
 */
class XrdHttpTpcTPCCheck {
public:
  /**
   * Tells whether a COPY request would copy a file onto itself. The destination
   * is truncated before a single byte has been read back from the source, so
   * such a request destroys the file and has to be rejected.
   *
   * The path must match, and the remote end must be this very server. The
   * latter is told by two signals with complementary blind spots, either of
   * which is enough: the Host header, which the client writes just like the
   * remote URL but which it may omit, and the address the request came in on,
   * which no header can influence and which sees through DNS aliases.
   *
   * @param remoteURL the other end of the transfer: the Source header of a pull
   *        or the Destination header of a push
   * @param requestPath the unquoted path the COPY request is about
   * @param requestHost the Host header, empty when the request carries none
   * @param requestSocket the socket the request came in on, negative if unknown
   * @return true if both ends of the COPY designate the same file
   */
  static bool isCopyOntoItself(const std::string & remoteURL,
                               const std::string & requestPath,
                               const std::string & requestHost,
                               int requestSocket);

private:
  /// The parts of a TPC URL that tell which file it designates.
  struct Endpoint {
    std::string authority;   //!< host[:port], lowercased, default port dropped
    std::string hostPort;    //!< host[:port] as written, for name resolution
    std::string path;        //!< path, without the query string
    int defaultPort = 0;     //!< port of the scheme, when the URL carries none
  };

  /**
   * Splits "<scheme>://<authority>[/<path>][?<query>]". The authority is left as
   * written, as only XrdNetUtils needs the host and the port it is made of, and
   * it parses those itself.
   * @return false when the URL carries no authority
   */
  static bool splitURL(const std::string & url, Endpoint & endpoint);

  /**
   * Lowercases an authority and drops its port when it is the default port of
   * its scheme, so that "MyHost.cern.ch:443" and "myhost.cern.ch" compare equal
   * for an https URL. A server listening on any other port is named with that
   * port in the URL and in the Host header alike.
   */
  static std::string normalizeAuthority(const std::string & authority,
                                        int defaultPort);

  /// Tells whether the remote end is named like the server the request went to.
  static bool isSameAuthority(const Endpoint & remote,
                              const std::string & requestHost);

  /// Tells whether the remote end resolves to the address and port the request
  /// came in on.
  static bool isSameEndpoint(const Endpoint & remote, int requestSocket);
};

#endif //XROOTD_XRDHTTPTPCTPCCHECK_HH

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
   * The path must match, and the remote end must be this very server: either
   * it is named like the Host header, or any of its addresses and port matches
   * one of ours. Ours are those the Host header resolves to and the one the
   * request came in on, which no header can influence.
   *
   * With tpc.dfs, the servers of its domain share our filesystem, so the remote
   * end may also be any of them.
   *
   * @param remoteURL the other end of the transfer: the Source header of a pull
   *        or the Destination header of a push
   * @param requestPath the unquoted path the COPY request is about
   * @param requestHost the Host header, empty when the request carries none
   * @param requestSocket the socket the request came in on, negative if unknown
   * @param dfsDomain the tpc.dfs domain, lowercased, empty when not configured
   * @param errMsg set to why the COPY has to be rejected, if not null
   * @return true if both ends of the COPY designate the same file
   */
  static bool isCopyOntoItself(const std::string & remoteURL,
                               const std::string & requestPath,
                               const std::string & requestHost,
                               int requestSocket,
                               const std::string & dfsDomain = "",
                               std::string * errMsg = nullptr);

  /// Tells whether a lowercased tpc.dfs domain is a plain domain name: labels
  /// of letters, digits and '-' separated by single dots, and no IP address.
  static bool isValidDomain(const std::string & domain);

  /// Tells whether a host name is the lowercased domain or ends with ".domain".
  static bool isInDomain(const std::string & hostName, const std::string & domain);

  /**
   * Tells whether a push may upload. With "Overwrite: F", only a destination the
   * HEAD request found missing (404) is safe: any 2xx means it exists, anything
   * else that its existence could not be verified.
   *
   * @param overwrite false when the COPY forbids overwriting ("Overwrite: F")
   * @param headStatus the final status of the HEAD request sent to the
   *        destination, negative when no response came back
   * @param errMsg set to why the push has to be rejected, if not null
   */
  static bool isPushAllowed(bool overwrite, int headStatus,
                            std::string * errMsg = nullptr);

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

  /// Tells whether any address and port of the remote end is one of ours.
  static bool isSameEndpoint(const Endpoint & remote,
                             const std::string & requestHost,
                             int requestSocket);

  /// Tells whether the remote end is in the tpc.dfs domain, by its name or by
  /// the names of its addresses. An address with no name is assumed to be.
  static bool isOnSharedFilesystem(const Endpoint & remote,
                                   const std::string & domain,
                                   std::string * errMsg);

  /// The host of "host[:port]", an IPv6 address keeping its brackets.
  static std::string hostOf(const std::string & hostPort);
};

#endif //XROOTD_XRDHTTPTPCTPCCHECK_HH

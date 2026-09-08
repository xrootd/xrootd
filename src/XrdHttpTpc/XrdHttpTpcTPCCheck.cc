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

#include "XrdHttpTpcTPCCheck.hh"

#include "XrdHttp/XrdHttpUtils.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include <algorithm>
#include <cctype>
#include <vector>

bool XrdHttpTpcTPCCheck::isCopyOntoItself(const std::string & remoteURL,
                                          const std::string & requestPath,
                                          const std::string & requestHost,
                                          int requestSocket) {
  Endpoint remote;
  if (!splitURL(remoteURL, remote)) {
    return false;
  }

  // XrdHttpReq::parseResource() unquotes the path of the request and squashes
  // its duplicate slashes, the URL of the remote end is raw. Tested first, so
  // that the name resolution below only happens for a suspicious request.
  if (XrdOucUtils::NormalizePath(decode_str(remote.path)) !=
      XrdOucUtils::NormalizePath(requestPath)) {
    return false;
  }

  return isSameAuthority(remote, requestHost) ||
         isSameEndpoint(remote, requestSocket);
}

bool XrdHttpTpcTPCCheck::splitURL(const std::string & url, Endpoint & endpoint) {
  auto schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos) {
    return false;
  }

  std::string scheme = url.substr(0, schemeEnd);
  std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  endpoint.defaultPort = (scheme == "https") ? 443 : 80;

  auto authorityStart = schemeEnd + 3;
  auto pathStart = url.find('/', authorityStart);

  if (pathStart == std::string::npos) {
    endpoint.hostPort = url.substr(authorityStart);
    endpoint.path.clear();
  } else {
    endpoint.hostPort = url.substr(authorityStart, pathStart - authorityStart);
    // The query string carries opaque information, not the identity of the file
    endpoint.path = url.substr(pathStart, url.find('?', pathStart) - pathStart);
  }

  // "user:password@host" does not identify the server, only "host" does
  auto userInfoEnd = endpoint.hostPort.rfind('@');
  if (userInfoEnd != std::string::npos) {
    endpoint.hostPort.erase(0, userInfoEnd + 1);
  }

  endpoint.authority = normalizeAuthority(endpoint.hostPort, endpoint.defaultPort);
  return !endpoint.hostPort.empty();
}

std::string XrdHttpTpcTPCCheck::normalizeAuthority(const std::string & authority,
                                                   int defaultPort) {
  std::string normalized(authority);

  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  const std::string port = ":" + std::to_string(defaultPort);
  if (normalized.size() > port.size() &&
      normalized.compare(normalized.size() - port.size(), port.size(), port) == 0) {
    normalized.resize(normalized.size() - port.size());
  }

  return normalized;
}

bool XrdHttpTpcTPCCheck::isSameAuthority(const Endpoint & remote,
                                         const std::string & requestHost) {
  if (requestHost.empty()) {
    return false;
  }

  return remote.authority == normalizeAuthority(requestHost, remote.defaultPort);
}

bool XrdHttpTpcTPCCheck::isSameEndpoint(const Endpoint & remote, int requestSocket) {
  if (requestSocket < 0) {
    return false;
  }

  // The address the client reached us on, not the address of the client itself
  XrdNetAddr localAddr;
  if (localAddr.Set(requestSocket, false)) {
    return false;
  }

  std::vector<XrdNetAddr> remoteAddrs;
  // A negative port is only used when the specification does not carry one
  if (XrdNetUtils::GetAddrs(remote.hostPort, remoteAddrs, nullptr,
                            XrdNetUtils::allIPMap, -remote.defaultPort)) {
    return false;
  }

  for (auto & remoteAddr : remoteAddrs) {
    if (remoteAddr.Same(&localAddr, true)) {
      return true;
    }
  }

  return false;
}

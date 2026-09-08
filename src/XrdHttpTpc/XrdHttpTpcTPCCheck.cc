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
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include <algorithm>
#include <cctype>
#include <vector>

bool XrdHttpTpcTPCCheck::isCopyOntoItself(const std::string & remoteURL,
                                          const std::string & requestPath,
                                          const std::string & requestHost,
                                          int requestSocket,
                                          const std::string & dfsDomain,
                                          std::string * errMsg) {
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

  if (isSameAuthority(remote, requestHost) ||
      isSameEndpoint(remote, requestHost, requestSocket)) {
    if (errMsg) {
      *errMsg = "the source and the destination are the same file";
    }
    return true;
  }

  return !dfsDomain.empty() && isOnSharedFilesystem(remote, dfsDomain, errMsg);
}

bool XrdHttpTpcTPCCheck::isValidDomain(const std::string & domain) {
  if (domain.empty() || domain.front() == '.' || domain.back() == '.' ||
      domain.find("..") != std::string::npos) {
    return false;
  }

  for (unsigned char c : domain) {
    if (!std::islower(c) && !std::isdigit(c) && c != '-' && c != '.') {
      return false;
    }
  }

  // "192.168.1.0" is made of valid characters, but it is an address
  return XrdNetAddrInfo::isHostName(domain.c_str());
}

bool XrdHttpTpcTPCCheck::isInDomain(const std::string & hostName,
                                    const std::string & domain) {
  std::string host(hostName);
  XrdOucUtils::toLower(&host[0]);
  if (!host.empty() && host.back() == '.') {
    host.pop_back();
  }

  if (domain.empty() || host.size() < domain.size() ||
      host.compare(host.size() - domain.size(), domain.size(), domain) != 0) {
    return false;
  }

  // "example.org" holds "srv.example.org", not "badexample.org"
  return host.size() == domain.size() ||
         host[host.size() - domain.size() - 1] == '.';
}

bool XrdHttpTpcTPCCheck::isPushAllowed(bool overwrite, int headStatus,
                                       std::string * errMsg) {
  if (overwrite || headStatus == 404) {
    return true;
  }

  if (errMsg) {
    if (headStatus >= 200 && headStatus < 300) {
      *errMsg = "the header 'Overwrite: F' was supplied and the destination "
                "already exists, so the transfer is failed";
    } else {
      const std::string answer = (headStatus < 0
        ? "got no response"
        : "returned " + std::to_string(headStatus));
      *errMsg = "the header 'Overwrite: F' was supplied but the existence of "
                "the destination could not be verified (the HEAD request to it " +
                answer + "), so the transfer is failed";
    }
  }
  return false;
}

bool XrdHttpTpcTPCCheck::isOnSharedFilesystem(const Endpoint & remote,
                                              const std::string & domain,
                                              std::string * errMsg) {
  auto shared = [&]() {
    if (errMsg) {
      *errMsg = "the source and the destination are the same file of the "
                "filesystem shared by the servers of " + domain;
    }
    return true;
  };

  const std::string host = hostOf(remote.hostPort);
  const bool isName = XrdNetAddrInfo::isHostName(host.c_str());

  if (isName && isInDomain(host, domain)) {
    return shared();
  }

  // Unmapped IPv4 addresses: glibc does not look a mapped one up in the IPv4
  // entries of /etc/hosts. Left empty when the host does not resolve.
  std::vector<XrdNetAddr> addrs;
  XrdNetUtils::GetAddrs(remote.hostPort, addrs, nullptr, XrdNetUtils::allIPv64,
                        -remote.defaultPort);

  if (isName) {
    // An alias out of the domain may still name one of its servers. A name that
    // does not resolve cannot be reached by the transfer either.
    for (auto & addr : addrs) {
      const char * name = addr.Name();
      if (name && isInDomain(name, domain)) {
        return shared();
      }
    }
    return false;
  }

  // Name() falls back to the address itself when it has no name. Without a
  // name, whether it shares our filesystem cannot be told: assume it does.
  const char * name = (addrs.empty() ? nullptr : addrs[0].Name());
  if (!name || !XrdNetAddrInfo::isHostName(name)) {
    if (errMsg) {
      *errMsg = "the source and the destination have the same path, and the "
                "address " + host + " has no name telling whether it shares "
                "the filesystem of the servers of " + domain;
    }
    return true;
  }

  return isInDomain(name, domain) && shared();
}

std::string XrdHttpTpcTPCCheck::hostOf(const std::string & hostPort) {
  if (!hostPort.empty() && hostPort.front() == '[') {
    auto end = hostPort.find(']');
    return hostPort.substr(0, end == std::string::npos ? end : end + 1);
  }
  return hostPort.substr(0, hostPort.find(':'));
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

bool XrdHttpTpcTPCCheck::isSameEndpoint(const Endpoint & remote,
                                        const std::string & requestHost,
                                        int requestSocket) {
  // Our addresses: all those the Host header resolves to, and the one the client
  // reached us on (not the address of the client itself). A negative port is
  // only used when the specification does not carry one.
  std::vector<XrdNetAddr> localAddrs;
  if (!requestHost.empty()) {
    XrdNetUtils::GetAddrs(requestHost, localAddrs, nullptr,
                          XrdNetUtils::allIPMap, -remote.defaultPort);
  }

  XrdNetAddr socketAddr;
  if (requestSocket >= 0 && !socketAddr.Set(requestSocket, false)) {
    localAddrs.push_back(socketAddr);
  }

  if (localAddrs.empty()) {
    return false;
  }

  std::vector<XrdNetAddr> remoteAddrs;
  if (XrdNetUtils::GetAddrs(remote.hostPort, remoteAddrs, nullptr,
                            XrdNetUtils::allIPMap, -remote.defaultPort)) {
    return false;
  }

  for (auto & remoteAddr : remoteAddrs) {
    for (auto & localAddr : localAddrs) {
      if (remoteAddr.Same(&localAddr, true)) {
        return true;
      }
    }
  }

  return false;
}

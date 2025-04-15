//------------------------------------------------------------------------------
// This file is part of XrdHttpTpcTPC
//
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <ccaffy@cern.ch>
// File Date: Oct 2023
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
#include "XrdHttpTpcUtils.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdHttpTpc/XrdHttpTpcTPC.hh"

#include <sstream>

std::string XrdHttpTpcUtils::prepareOpenURL(const std::string & reqResource, std::map<std::string,std::string> & reqHeaders, const std::map<std::string,std::string> & hdr2cgimap) {
  auto iter = XrdOucTUtils::caseInsensitiveFind(reqHeaders,"xrd-http-query");
  std::stringstream opaque;

  // https://github.com/xrootd/xrootd/issues/2427 we tell the oss layer that this transfer is a HTTP TPC one
  opaque << "?" << TPC::TPCHandler::OSS_TASK_OPAQUE;

  if (iter != reqHeaders.end() && !iter->second.empty()) {
    std::string token;
    std::istringstream requestStream(iter->second);
    auto has_authz_header = XrdOucTUtils::caseInsensitiveFind(reqHeaders,"authorization") != reqHeaders.end();
    while (std::getline(requestStream, token, '&')) {
      if (token.empty()) {
        continue;
      } else if (!strncmp(token.c_str(), "authz=", 6)) {
        if (!has_authz_header) {
          reqHeaders["Authorization"] = token.substr(6);
          has_authz_header = true;
        }
      } else {
        opaque << "&" << token;
      }
    }
  }

  // Append CGI coming from the tpc.header2cgi parameter
  for(auto & hdr2cgi : hdr2cgimap) {
    auto it = std::find_if(reqHeaders.begin(),reqHeaders.end(),[&hdr2cgi](const auto & elt){
      return !strcasecmp(elt.first.c_str(),hdr2cgi.first.c_str());
    });
    if(it != reqHeaders.end()) {
      opaque << "&" << hdr2cgi.second << "=" << it->second;
    }
  }

  return reqResource + opaque.str();
}
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

std::string XrdHttpTpcUtils::prepareOpenURL(PrepareOpenURLParams & params) {
  auto iter = XrdOucTUtils::caseInsensitiveFind(params.reqHeaders,"xrd-http-query");
  std::stringstream opaque;

  // https://github.com/xrootd/xrootd/issues/2427 we tell the oss layer that this transfer is a HTTP TPC one
  opaque << "?" << TPC::TPCHandler::OSS_TASK_OPAQUE;

  if (iter != params.reqHeaders.end() && !iter->second.empty()) {
    std::string token;
    std::istringstream requestStream(iter->second);
    auto has_authz_header = XrdOucTUtils::caseInsensitiveFind(params.reqHeaders,"authorization") != params.reqHeaders.end();
    while (std::getline(requestStream, token, '&')) {
      if (token.empty()) {
        continue;
      } else if (!strncmp(token.c_str(), "authz=", 6)) {
        if (!has_authz_header) {
          params.reqHeaders["Authorization"] = token.substr(6);
          has_authz_header = true;
        }
      } else {
        opaque << "&" << token;
      }
    }
  }

  // Append CGI coming from the tpc.header2cgi parameter
  for(auto & hdr2cgi : params.hdr2cgimap) {
    auto it = std::find_if(params.reqHeaders.begin(),params.reqHeaders.end(),[&hdr2cgi](const auto & elt){
      return !strcasecmp(elt.first.c_str(),hdr2cgi.first.c_str());
    });
    if(it != params.reqHeaders.end()) {
      opaque << "&" << hdr2cgi.second << "=" << it->second;
    }
  }

  // Append CGI coming from repr-digest header
  if(!params.reprDigest.empty()) {
    // We take the first alphabetically ordered Repr-Digest
    const auto & [digestName,digestValue] = *params.reprDigest.begin();
    opaque << "&cks.type=" << digestName << "&cks.value=" << digestValue;
  }


  return params.reqResource + opaque.str();
}
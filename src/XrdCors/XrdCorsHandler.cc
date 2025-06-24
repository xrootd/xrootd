//------------------------------------------------------------------------------
// This file is part of the XRootD framework
//
// Copyright (c) 2025 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <cedric.caffy@cern.ch>
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

#include "XrdCorsHandler.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdVersion.hh"
#include "XrdOuc/XrdOucUtils.hh"

extern "C"
{
XrdCors *XrdCorsGetHandler(XrdCorsGetHandlerArgs) {
  return new XrdCorsHandler();
}
}

/**
 * Configure the CORS handler
 * @param configFN the configuration file name
 * @param errP the error pointer allowing to log potential errors
 * @return 0 if successful, 1 otherwise
 */
int XrdCorsHandler::Configure(const char *configFN, XrdSysError *errP) {
  // Get the cors.origin parameters which can be either space-delimited or a repetition of cors.origin
  XrdOucGatherConf gconf("cors.origin",errP);
  if(gconf.Gather(configFN,XrdOucGatherConf::only_body) < 0){
    return 1;
  }
  if(gconf.GetLine()) {
    while(char * val = gconf.GetToken()) {
      // No need to check for correctness
      addAllowedOrigin(val);
    }
  }
  return 0;
}

std::optional<std::string> XrdCorsHandler::getCORSAllowOriginHeader(const std::string & origin) {
  auto originItor = m_origins.find(origin);
  if(originItor != m_origins.end()) {
    return "Access-Control-Allow-Origin: " + origin;
  }
  // We did not find any allowed origin, return nullopt
  return std::nullopt;
}

void XrdCorsHandler::addAllowedOrigin(std::string_view origin) {
  XrdOucUtils::trim(origin);
  if(!origin.empty()) {
    m_origins.emplace(origin);
  }
}

XrdVERSIONINFO(XrdCorsget,XrdCorsHandler>);
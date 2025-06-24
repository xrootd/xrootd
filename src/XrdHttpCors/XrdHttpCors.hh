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

#ifndef __XRD_HTTP_CORS_HEADERS__
#define __XRD_HTTP_CORS_HEADERS__

#include <map>
#include <string>
#include <string_view>
#include <optional>


class XrdOucEnv;
class XrdOucErrInfo;
class XrdSysError;

/**
 * Base class for CORS plugin implementation
 */
class XrdHttpCors {
public:
  /**
   * Configure the CORS plugin
   * @param configFN the server configuration file path
   * @param errP the pointer to the error object
   * @return 0 if the configuration is successful, 1 otherwise
   */
  virtual int Configure(const char * configFN, XrdSysError *errP) = 0;
  /**
   * Add trusted origins to the CORS plugin
   * @param origin the trusted origin to add
   */
  virtual void addAllowedOrigin(const std::string & origin) = 0;
  /**
   * Returns the fully formed Access-Control-Allow-Origin header.
   * If the origin passed in parameter matches one of the previously added origins, it will return
   * "Access-Control-Allow-Origin: matchedOrigin".
   * If the origin passed in parameter does not match any of the previously added origins, then std::nullopt will
   * be returned
   * @param origin
   * @return either the fully formed Access-Control-Allow-Origin header or nullopt if the origin does not match any added origin
   */
  virtual std::optional<std::string>  getCORSAllowOriginHeader(const std::string & origin) = 0;
};

typedef XrdHttpCors *(*XrdHttpCorsget_t)(XrdSysError *eDest,
                                             const char  *confg,
                                             XrdOucEnv   *envP
);

// Define arguments to get the Cors handler here
#define XrdHttpCorsGetHandlerArgs

extern "C" XrdHttpCors* XrdHttpCorsGetHandler(XrdHttpCorsGetHandlerArgs);

#endif


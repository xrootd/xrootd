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

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <optional>


class XrdOucEnv;
class XrdOucErrInfo;
class XrdSysError;

class XrdCors {
public:
  virtual int Configure(const char * configFN, XrdSysError *errP) = 0;
  virtual void addAllowedOrigin(std::string_view origin) = 0;
  virtual std::optional<std::string>  getCORSAllowOriginHeader(const std::string & origin) = 0;
};

typedef XrdCors *(*XrdCorsget_t)(XrdSysError *eDest,
                                             const char  *confg,
                                             XrdOucEnv   *envP
);

#define XrdCorsGetHandlerArgs

extern "C" XrdCors* XrdCorsGetHandler(XrdCorsGetHandlerArgs);


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

#ifndef XROOTD_XRDCORSHANDLER_HH
#define XROOTD_XRDCORSHANDLER_HH

#include "XrdCors/XrdCors.hh"
#include <unordered_set>

class XrdCorsHandler : public XrdCors {
public:
  XrdCorsHandler() = default;
  void addAllowedOrigin(std::string_view origin) override;
  int Configure(const char * configFN, XrdSysError *errP) override;
  std::optional<std::string> getCORSAllowOriginHeader(const std::string & origin) override;
private:
  std::unordered_set<std::string> m_origins;
};


#endif //XROOTD_XRDCORSHANDLER_HH

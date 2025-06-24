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

#ifndef __XROOTD_XRDHTTPCORSHANDLER_HH__
#define __XROOTD_XRDHTTPCORSHANDLER_HH__

#include "XrdHttpCors/XrdHttpCors.hh"
#include <unordered_set>

/**
 * Basic CORS plugin implementation
 */
class XrdHttpCorsHandler : public XrdHttpCors {
public:
  XrdHttpCorsHandler() = default;
  void addAllowedOrigin(const std::string & origin) override;
  int Configure(const char * configFN, XrdSysError *errP) override;
  std::optional<std::string> getCORSAllowOriginHeader(const std::string & origin) override;
private:
  std::unordered_set<std::string> m_origins;
};


#endif //__XROOTD_XRDHTTPCORSHANDLER_HH__

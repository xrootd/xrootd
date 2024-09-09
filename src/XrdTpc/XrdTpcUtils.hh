//------------------------------------------------------------------------------
// This file is part of XrdTpcTPC
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

#ifndef XROOTD_XRDTPCUTILS_HH
#define XROOTD_XRDTPCUTILS_HH

#include <string>
#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdTpcUtils {
public:
  /**
   * Prepares the file XRootD open URL from the request resource, the xrd-http-query header of the HTTP request and the hdr2cgi map passed in parameter
   * @param reqResource the HTTP request resource
   * @param the HTTP request headers
   * @param hdr2cgimap the map containing header keys --> XRootD cgi mapping
   * @param hasSetOpaque is set to true if the returned URL contains opaque query, false otherwise
   * @return the XRootD open URL
   */
  static std::string prepareOpenURL(const std::string & reqResource, std::map<std::string,std::string> & reqHeaders, const std::map<std::string,std::string> & hdr2cgimap, bool & hasSetOpaque);
};


#endif //XROOTD_XRDTPCUTILS_HH

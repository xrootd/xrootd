//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <ccaffy@cern.ch>
// File Date: Mar 2023
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

#ifndef XROOTD_XRDHTTPCHECKSUM_HH
#define XROOTD_XRDHTTPCHECKSUM_HH

#include <string>

/**
 * Simple object containing information about
 * a checksum
 */
class XrdHttpChecksum {
public:
    /**
     * Constructor
     * @param xrootConfigDigestName the name that will be used by XRootD server to run the checksum
     * @param httpName the HTTP RFC compliant name of the checksum
     * @param needsBase64Padding sets to true if the checksum needs to be base64 encoded before being sent, false otherwise
     */
    XrdHttpChecksum(const std::string & xrootConfigDigestName, const std::string & httpName, bool needsBase64Padding);

    std::string getXRootDConfigDigestName() const;
    std::string getHttpName() const;
    std::string getHttpNameLowerCase() const;
    bool needsBase64Padding() const;

private:
    std::string mXRootDConfigDigestName;
    std::string mHTTPName;
    bool mNeedsBase64Padding;
};


#endif //XROOTD_XRDHTTPCHECKSUM_HH

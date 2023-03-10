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

#include "XrdHttpChecksum.hh"
#include <algorithm>

XrdHttpChecksum::XrdHttpChecksum(const std::string & xrootConfigDigestName, const std::string & httpName, bool needsBase64Padding):
        mXRootDConfigDigestName(xrootConfigDigestName),
        mHTTPName(httpName),
        mNeedsBase64Padding(needsBase64Padding){}

std::string XrdHttpChecksum::getHttpName() const {
    return mHTTPName;
}

std::string XrdHttpChecksum::getHttpNameLowerCase() const {
    std::string ret = getHttpName();
    std::transform(ret.begin(),ret.end(),ret.begin(),::tolower);
    return ret;
}

std::string XrdHttpChecksum::getXRootDConfigDigestName() const {
    return mXRootDConfigDigestName;
}

bool XrdHttpChecksum::needsBase64Padding() const {
    return mNeedsBase64Padding;
}
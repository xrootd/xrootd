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

#include "XrdHttpChecksumHandler.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include <exception>
#include <algorithm>

std::map<std::string,XrdHttpChecksumHandlerImpl::XrdHttpChecksumPtr> XrdHttpChecksumHandlerImpl::XROOTD_DIGEST_NAME_TO_CKSUMS;

void XrdHttpChecksumHandlerImpl::initializeCksumsMaps() {
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("md5","md5",true));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("adler32","adler32",false));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("sha1","sha",true));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("sha256","sha-256",true));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("sha512","sha-512",true));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("cksum","UNIXcksum",false));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("crc32","crc32",false));
    addChecksumToMaps(std::make_unique<XrdHttpChecksum>("crc32c","crc32c",true));
}

void XrdHttpChecksumHandlerImpl::addChecksumToMaps(XrdHttpChecksumHandlerImpl::XrdHttpChecksumPtr && checksum) {
    // We also map xrootd-configured checksum's HTTP names to the corresponding checksums --> this will allow
    // users to configure algorithms like, for example, `sha-512` and be considered as `sha512` algorithm
    XROOTD_DIGEST_NAME_TO_CKSUMS[checksum->getHttpNameLowerCase()] = std::make_unique<XrdHttpChecksum>(checksum->getHttpNameLowerCase(), checksum->getHttpName(), checksum->needsBase64Padding());
    XROOTD_DIGEST_NAME_TO_CKSUMS[checksum->getXRootDConfigDigestName()] = std::move(checksum);
}

XrdHttpChecksumHandlerImpl::XrdHttpChecksumRawPtr XrdHttpChecksumHandlerImpl::getChecksumToRun(const std::string &userDigestIn) const {
    if(!mConfiguredChecksums.empty()) {
        std::vector<std::string> userDigests = getUserDigests(userDigestIn);
        //Loop over the user digests and find the corresponding checksum
        for(auto userDigest: userDigests) {
            auto httpCksum = std::find_if(mConfiguredChecksums.begin(), mConfiguredChecksums.end(),[userDigest](const XrdHttpChecksumRawPtr & cksum){
                return userDigest == cksum->getHttpNameLowerCase();
            });
            if(httpCksum != mConfiguredChecksums.end()) {
                return *httpCksum;
            }
        }
        return mConfiguredChecksums[0];
    }
    //If there are no configured checksums, return nullptr
    return nullptr;
}

const std::vector<std::string> &XrdHttpChecksumHandlerImpl::getNonIANAConfiguredCksums() const {
    return mNonIANAConfiguredChecksums;
}

const std::vector<XrdHttpChecksumHandler::XrdHttpChecksumRawPtr> & XrdHttpChecksumHandlerImpl::getConfiguredChecksums() const {
    return mConfiguredChecksums;
}


void XrdHttpChecksumHandlerImpl::configure(const char *csList) {
    initializeCksumsMaps();
    if(csList != nullptr) {
        initializeXRootDConfiguredCksums(csList);
    }
}

void XrdHttpChecksumHandlerImpl::initializeXRootDConfiguredCksums(const char *csList) {
    std::vector<std::string> splittedCslist;
    XrdOucTUtils::splitString(splittedCslist,csList,",");
    for(auto csElt: splittedCslist) {
        auto csName = getElement(csElt,":",1);
        auto checksumItor = XROOTD_DIGEST_NAME_TO_CKSUMS.find(csName);
        if(checksumItor != XROOTD_DIGEST_NAME_TO_CKSUMS.end()) {
            mConfiguredChecksums.push_back(checksumItor->second.get());
        } else {
            mNonIANAConfiguredChecksums.push_back(csName);
        }
    }
}

std::string XrdHttpChecksumHandlerImpl::getElement(const std::string &input, const std::string & delimiter,
                                               const size_t position) {
    std::vector<std::string> elementsAfterSplit;
    XrdOucTUtils::splitString(elementsAfterSplit,input,delimiter);
    return elementsAfterSplit[position];
}

std::vector<std::string> XrdHttpChecksumHandlerImpl::getUserDigests(const std::string &userDigests) {
    //userDigest is a comma-separated list with q-values
    std::vector<std::string> userDigestsRet;
    std::vector<std::string> userDigestsWithQValues;
    XrdOucTUtils::splitString(userDigestsWithQValues,userDigests,",");
    for(auto & userDigestWithQValue: userDigestsWithQValues){
        std::transform(userDigestWithQValue.begin(),userDigestWithQValue.end(),userDigestWithQValue.begin(),::tolower);
        auto userDigest = getElement(userDigestWithQValue,";",0);
        XrdOucUtils::trim(userDigest);
        userDigestsRet.push_back(userDigest);
    }
    return userDigestsRet;
}
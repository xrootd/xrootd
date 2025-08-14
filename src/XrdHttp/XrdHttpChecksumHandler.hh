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
#ifndef XROOTD_XRDHTTPCHECKSUMHANDLER_HH
#define XROOTD_XRDHTTPCHECKSUMHANDLER_HH

#include "XrdHttpChecksum.hh"

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <cstdint>

/**
 * Implementation class of the XrdHttpChecksumHandler
 *
 * Is useful for unit testing
 */
class XrdHttpChecksumHandlerImpl {
public:
    using XrdHttpChecksumPtr = std::unique_ptr<XrdHttpChecksum>;
    using XrdHttpChecksumRawPtr = XrdHttpChecksum *;

    XrdHttpChecksumHandlerImpl() = default;

    void configure(const char * csList);
    XrdHttpChecksumRawPtr getChecksumToRunWantDigest(const std::string & wantDigest) const;
    XrdHttpChecksumRawPtr getChecksumToRunWantReprDigest(const std::map<std::string,uint8_t> & wantReprDigest) const;
    const std::vector<std::string> & getNonIANAConfiguredCksums() const;
    /**
     * For testing purposes
     */
    const std::vector<XrdHttpChecksumRawPtr> & getConfiguredChecksums() const;
private:
    /**
     * Initializes the checksums from the csList parameter passed
     *
     * The elements of the csList parameter should all be lower-cased
     * @param csList the list of the configured checksum under the format 0:adler32,1:sha1,2:sha512
     */
    void initializeXRootDConfiguredCksums(const char *csList);
    /**
     * Modify this if new checksums have to be supported or
     * if some don't require base64 padding anymore
     */
    static void initializeCksumsMaps();
    static void addChecksumToMaps(XrdHttpChecksumPtr && checksum);
    static std::string getElement(const std::string & input, const std::string & delimiter, const size_t position);
    /**
     * Returns a vector of user digests (lower-cased) extracted from the userDigests string passed in parameter
     * @param userDigests the string containing a quality-valued checksum list e.g: adler32, md5;q=0.4, md5
     * @return the lower-cased user digests vector
     */
    static std::vector<std::string> getUserDigests(const std::string & userDigests);

    //The map that containing all possible IANA-HTTP-compatible xrootd checksums
    static std::map<std::string,XrdHttpChecksumPtr> XROOTD_DIGEST_NAME_TO_CKSUMS;
    // The vector of IANA-HTTP-compatible configured checksum
    std::vector<XrdHttpChecksumRawPtr> mConfiguredChecksums;
    // The vector of non-HTTP configured checksum names (for testing purposes)
    std::vector<std::string> mNonIANAConfiguredChecksums;
};

/**
 * This class allows to handle xrd http checksum algorithm selection
 * based on what the user provided as a digest
 */
class XrdHttpChecksumHandler {
public:
    using XrdHttpChecksumRawPtr = XrdHttpChecksumHandlerImpl::XrdHttpChecksumRawPtr;

    XrdHttpChecksumHandler() = default;
    /**
     * Configure this handler.
     * @throws runtime_exception if no algorithm in the csList is compatible with HTTP
     * @param csList the list coming from the server configuration. Should be under the format 0:adler32,1:sha512
     */
    void configure(const char * csList) { pImpl.configure(csList); }
    /**
     * Returns the checksum to run from the user "Want-Digest" provided string
     * @param wantDigest the digest string under the format "sha-512,sha-256;q=0.8,sha;q=0.6,md5;q=0.4,adler32;q=0.2"
     * @return the checksum to run depending on the userDigest provided string
     * The logic behind it is simple: returns the first userDigest provided that matches the one configured.
     * If none is matched, the first algorithm configured on the server side will be returned.
     * If no HTTP-IANA compatible checksum algorithm has been configured or NO checksum algorithm have been configured, nullptr will be returned.
     */
    XrdHttpChecksumRawPtr getChecksumToRunWantDigest(const std::string & wantDigest) const { return pImpl.getChecksumToRunWantDigest(wantDigest); }

    /**
     * Returns the checksum to run from the user "Want-Repr-Digest" header provided values (already parsed in a map <digestName, preference>)
     * @param wantReprDigest the map containing the user want-repr-digest values
     * @return the checksum to run depending on the map in parameter.
     * If the map contains checksums that are not configured on the server, it will return the first configured checksum on the server.
     * If multiple configured checksums are configured and multiple checksums are requested, it will return the one with the highest preference.
     * If same preference, the checksum returned will be the first one of the alphabetically ordered configured checksum
     */
    XrdHttpChecksumRawPtr getChecksumToRunWantReprDigest(const std::map<std::string,uint8_t> & wantReprDigest) const { return pImpl.getChecksumToRunWantReprDigest(wantReprDigest); }

    /**
     * Returns the checksums that are incompatible with HTTP --> the ones that
     * we do not know whether the result should be base64 encoded or not
     */
    const std::vector<std::string> & getNonIANAConfiguredCksums() const { return pImpl.getNonIANAConfiguredCksums(); }
private:
    XrdHttpChecksumHandlerImpl pImpl;
};


#endif //XROOTD_XRDHTTPCHECKSUMHANDLER_HH

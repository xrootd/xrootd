/******************************************************************************/
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

// This file contains class definitions for the responses created by XrdClHttp.
// It is a public header, meant to be used by libraries that rely on Curl / HTTP
// -specific information such as header or trailers from the responses.

#include "XrdClHttpResponseInfo.hh"

#include <XrdCl/XrdClXRootDResponses.hh>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace XrdClHttp {

// If this property name is set on the XrdCl::FileSystem object to "true",
// then XrdClHttp will use the classes defined in this header as response
// objects.
//
// The XrdCl responses do not have a virtual table so, when deleted through
// the base pointer, the response info object will leak.  The owner of the
// FileSystem object is responsible for guaranteeing that `GetResponseInfo`
// is called and the allocated memory is released.
#define ResponseInfoProperty "XrdClResponseInfo"

// Response holding not only the directory listing but the
// cumulative response headers from each HTTP request that
// contributed to the listing.
class DirectoryListResponse : public XrdCl::DirectoryList {
public:
    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}

private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

// Response holding not only the stat info but the
// cumulative response headers from each HTTP request that
// contributed to the listing.
class StatResponse : public XrdCl::StatInfo {
public:
    StatResponse(const std::string &id, uint64_t size, uint32_t flags, uint64_t modTime)
      : XrdCl::StatInfo(id, size, flags, modTime) {}

    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}

private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

// Response holding not only the query response but any cumulative
// response headers from each HTTP request that contributed to the operation
//
// Note: XrdCl::Buffer has a virtual table so managing response is simpler; no need
// to explicitly cast back and forth
class QueryResponse : public XrdCl::Buffer {
public:
    virtual ~QueryResponse() {}

    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}
private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

class OpenResponseInfo {
public:
    OpenResponseInfo() {}
    OpenResponseInfo(OpenResponseInfo&&) = default;
    virtual ~OpenResponseInfo() {}

    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}
private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

class DeleteResponseInfo {
public:
    virtual ~DeleteResponseInfo() {}

    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}
private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

class MkdirResponseInfo {
public:
    virtual ~MkdirResponseInfo() {}

    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}
private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

class ReadResponseInfo : public XrdCl::ChunkInfo {
public:
    ReadResponseInfo(uint64_t off = 0, uint32_t len = 0, void *buff = 0)
      : XrdCl::ChunkInfo(off, len, buff) {}

    virtual ~ReadResponseInfo() {}

    std::unique_ptr<ResponseInfo> GetResponseInfo() {return std::move(m_response_info);}
    void SetResponseInfo(std::unique_ptr<ResponseInfo> info) {m_response_info = std::move(info);}
private:
    std::unique_ptr<ResponseInfo> m_response_info;
};

} // namespace XrdClHttp

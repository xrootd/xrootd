/**
 * This file is part of XrdClHttp
 */

#ifndef __HTTP_STAT_
#define __HTTP_STAT_

#include <davix.hpp>

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include <cstdint>
#include <string>

namespace XrdCl {

class StatInfo;
}

namespace Posix {

std::pair<DAVIX_FD*, XrdCl::XRootDStatus> Open(Davix::DavPosix& davix_client,
                                               const std::string& url,
                                               int flags, uint16_t timeout);

XrdCl::XRootDStatus Close(Davix::DavPosix& davix_client, DAVIX_FD* fd);

XrdCl::XRootDStatus MkDir(Davix::DavPosix& davix_client,
                          const std::string& path,
                          XrdCl::MkDirFlags::Flags flags,
                          XrdCl::Access::Mode mode, uint16_t timeout);

XrdCl::XRootDStatus RmDir(Davix::DavPosix& davix_client,
                          const std::string& path, uint16_t timeout);

std::pair<XrdCl::DirectoryList*, XrdCl::XRootDStatus> DirList(
    Davix::DavPosix& davix_client, const std::string& path, bool details,
    bool recursive, uint16_t timeout);

XrdCl::XRootDStatus Rename(Davix::DavPosix& davix_client,
                           const std::string& source, const std::string& dest,
                           uint16_t timeout);

XrdCl::XRootDStatus Stat(Davix::DavPosix& davix_client, const std::string& url,
                         uint16_t timeout, XrdCl::StatInfo* stat_info);

XrdCl::XRootDStatus Unlink(Davix::DavPosix& davix_client,
                           const std::string& url, uint16_t timeout);

std::pair<int, XrdCl::XRootDStatus> Read(Davix::DavPosix& davix_client,
                                         DAVIX_FD* fd, void* buffer,
                                         uint32_t size);

std::pair<int, XrdCl::XRootDStatus> PRead(Davix::DavPosix& davix_client,
                                          DAVIX_FD* fd, void* buffer,
                                          uint32_t size, uint64_t offset);

std::pair<int, XrdCl::XRootDStatus> PReadVec(Davix::DavPosix& davix_client,
                                             DAVIX_FD* fd,
                                             const XrdCl::ChunkList& chunks,
                                             void* buffer);

std::pair<int, XrdCl::XRootDStatus> PWrite(Davix::DavPosix& davix_client,
                                           DAVIX_FD* fd, uint64_t offset,
                                           uint32_t size, const void* buffer,
                                           uint16_t timeout);

}  // namespace Posix

#endif  // __HTTP_STAT_

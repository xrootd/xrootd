/**
 * This file is part of XrdClHttp
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "XrdClHttp/XrdClHttpPosix.hh"

#include "XProtocol/XProtocol.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClURL.hh"

#include "auth/davixx509cred.hpp"
#include "auth/davixauth.hpp"

#include <string>

namespace {

std::vector<std::string> SplitString(const std::string& input,
                                     const std::string& delimiter) {
  size_t start = 0;
  size_t end = 0;
  size_t length = 0;

  auto result = std::vector<std::string>{};

  do {
    end = input.find(delimiter, start);

    if (end != std::string::npos)
      length = end - start;
    else
      length = input.length() - start;

    if (length) result.push_back(input.substr(start, length));

    start = end + delimiter.size();
  } while (end != std::string::npos);

  return result;
}

void SetTimeout(Davix::RequestParams& params, time_t timeout) {
/*
 * At NERSC archive portal, we get error when setOperationTimeout()
 *
  if (timeout != 0) {
    struct timespec ts = {timeout, 0};
    params.setOperationTimeout(&ts);
  }
*/

  struct timespec ts = {0, 0};
  ts.tv_sec = 30;
  params.setConnectionTimeout(&ts);

  params.setOperationRetry(0);
  params.setOperationRetryDelay(2);
}

XrdCl::XRootDStatus FillStatInfo(const struct stat& stats, XrdCl::StatInfo* stat_info) {
  std::ostringstream data;
  if (S_ISDIR(stats.st_mode)) {
    data << stats.st_dev << " " << stats.st_size << " " 
         << (XrdCl::StatInfo::Flags::IsDir | XrdCl::StatInfo::Flags::IsReadable | 
             XrdCl::StatInfo::Flags::IsWritable | XrdCl::StatInfo::Flags::XBitSet)
         << " " << stats.st_mtime;
  }
  else {
    if (getenv("AWS_ACCESS_KEY_ID")) {
        data << stats.st_dev << " " << stats.st_size << " " 
             << XrdCl::StatInfo::Flags::IsReadable << " " << stats.st_mtime;
    }
    else {
        data << stats.st_dev << " " << stats.st_size << " " 
             << stats.st_mode << " " << stats.st_mtime;
    }

  }

  if (!stat_info->ParseServerResponse(data.str().c_str())) {
    return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errDataError);
  }

  return XrdCl::XRootDStatus();
}

// return NULL if no X509 proxy is found 
//Davix::X509Credential* LoadX509UserCredential() {
//  std::string myX509proxyFile;
//  if (getenv("X509_USER_PROXY") != NULL)
//    myX509proxyFile = getenv("X509_USER_PROXY");
//  else
//    myX509proxyFile = "/tmp/x509up_u" + std::to_string(geteuid());
//  
//  struct stat myX509proxyStat;
//  Davix::X509Credential* myX509proxy = NULL;
//  if (stat(myX509proxyFile.c_str(), &myX509proxyStat) == 0) {
//    myX509proxy = new Davix::X509Credential();
//    myX509proxy->loadFromFilePEM(myX509proxyFile.c_str(), myX509proxyFile.c_str(), "", NULL);
//  }
//  return myX509proxy;
//}

// see auth/davixauth.hpp
int LoadX509UserCredentialCallBack(void *userdata, 
                                   const Davix::SessionInfo &info,
                                   Davix::X509Credential *cert,
                                   Davix::DavixError **err) {
  std::string myX509proxyFile;
  if (getenv("X509_USER_PROXY") != NULL)
    myX509proxyFile = getenv("X509_USER_PROXY");
  else
    myX509proxyFile = "/tmp/x509up_u" + std::to_string(geteuid());
  
  struct stat myX509proxyStat;
  if (stat(myX509proxyFile.c_str(), &myX509proxyStat) == 0)
    return cert->loadFromFilePEM(myX509proxyFile.c_str(), myX509proxyFile.c_str(), "", err);
  else
    return 1;
}

void SetX509(Davix::RequestParams& params) {
  params.setClientCertCallbackX509(&LoadX509UserCredentialCallBack, NULL);

  //Davix::X509Credential* myX509proxy = LoadX509UserCredential();
  //if (myX509proxy != NULL) {
  //  params.setClientCertX509(*myX509proxy);
  //  delete myX509proxy;
  //}

  if (getenv("X509_CERT_DIR") != NULL)
    params.addCertificateAuthorityPath(getenv("X509_CERT_DIR"));
  else
    params.addCertificateAuthorityPath("/etc/grid-security/certificates");      
}

void SetAuthS3(Davix::RequestParams& params) {
  //Davix::setLogScope(DAVIX_LOG_SCOPE_ALL);
  //Davix::setLogScope(DAVIX_LOG_HEADER | DAVIX_LOG_S3);
  //Davix::setLogLevel(DAVIX_LOG_TRACE);
  params.setProtocol(Davix::RequestProtocol::AwsS3);
  params.setAwsAuthorizationKeys(getenv("AWS_SECRET_ACCESS_KEY"),
                                 getenv("AWS_ACCESS_KEY_ID"));
  params.setAwsAlternate(true);
  // if AWS region is not set, Davix will use the old AWS signature v2
  if (getenv("AWS_REGION")) 
     params.setAwsRegion(getenv("AWS_REGION"));
  else if (! getenv("AWS_SIGNATURE_V2"))
     params.setAwsRegion("mars");
}

void SetAuthz(Davix::RequestParams& params) {
  if (getenv("AWS_ACCESS_KEY_ID") && getenv("AWS_SECRET_ACCESS_KEY"))
    SetAuthS3(params);
  else
    SetX509(params);
}

std::string SanitizedURL(const std::string& url) {
  XrdCl::URL xurl(url);
  std::string path = xurl.GetPath();
  if (path.find("/") != 0) path = "/" + path;
  std::string returl = xurl.GetProtocol() + "://" 
                     + xurl.GetHostName() + ":"
                     + std::to_string(xurl.GetPort())
                     + path;
  // for s3 storage using AWS_ACCESS_KEY_ID, filter out all CGIs
  // Known issues:
  // Google cloud storage does not like ?xrd.gsiusrpxy=/tmp/..., Will fail Stat()
  if (! getenv("AWS_ACCESS_KEY_ID") && ! xurl.GetParamsAsString().empty()) {
    returl = returl + xurl.GetParamsAsString();
  }
  return returl;
}

// check davix/include/davix/status/davixstatusrequest.hpp and
// XProtocol/XProtocol.hh (XErrorCode) for corresponding error codes.
std::pair<uint16_t, XErrorCode> ErrCodeConvert(Davix::StatusCode::Code code) {
  if (code == Davix::StatusCode::FileNotFound)
    return std::make_pair(XrdCl::errErrorResponse, kXR_NotFound);
  else if (code == Davix::StatusCode::FileExist)
    return std::make_pair(XrdCl::errErrorResponse, kXR_ItExists);
  else if (code == Davix::StatusCode::PermissionRefused)
    return std::make_pair(XrdCl::errErrorResponse, kXR_NotAuthorized);
  else
    return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);  
}

}  // namespace

namespace Posix {

using namespace XrdCl;

std::pair<DAVIX_FD*, XRootDStatus> Open(Davix::DavPosix& davix_client,
                                        const std::string& url, int flags,
                                        time_t timeout) {
  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);
  Davix::DavixError* err = nullptr;
  DAVIX_FD* fd = davix_client.open(&params, SanitizedURL(url), flags, &err);
  XRootDStatus status;
  if (!fd) {
    auto res = ErrCodeConvert(err->getStatus());
    status = XRootDStatus(stError, res.first, res.second, err->getErrMsg());
    delete err;
  }
  else {
    status = XRootDStatus();
  }
  return std::make_pair(fd, status);
}

XRootDStatus Close(Davix::DavPosix& davix_client, DAVIX_FD* fd) {
  Davix::DavixError* err = nullptr;
  if (davix_client.close(fd, &err)) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return errStatus;
  }

  return XRootDStatus();
}

XRootDStatus MkDir(Davix::DavPosix& davix_client, const std::string& path,
                   XrdCl::MkDirFlags::Flags flags, XrdCl::Access::Mode /*mode*/,
                   time_t timeout) {

  return XRootDStatus();

  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);

  auto DoMkDir = [&davix_client, &params](const std::string& path) {
    Davix::DavixError* err = nullptr;
    if (davix_client.mkdir(&params, SanitizedURL(path), S_IRWXU, &err) &&
        (err->getStatus() != Davix::StatusCode::FileExist)) {
      auto errStatus = XRootDStatus(stError, errInternal, err->getStatus(),
                                    err->getErrMsg());
      delete err;
      return errStatus;
    } else {
      return XRootDStatus();
    }
  };

  auto url = XrdCl::URL(path);

  if (flags & XrdCl::MkDirFlags::MakePath) {
    // Also create intermediate directories

    auto dirs = SplitString(url.GetPath(), "/");

    std::string dirs_cumul;
    for (const auto& d : dirs) {
      dirs_cumul += "/" + d;
      url.SetPath(dirs_cumul);
      auto status = DoMkDir(url.GetLocation());
      if (status.IsError()) {
        return status;
      }
    }
  } else {
    // Only create final directory
    auto status = DoMkDir(url.GetURL());
    if (status.IsError()) {
      return status;
    }
  }

  return XRootDStatus();
}

XRootDStatus RmDir(Davix::DavPosix& davix_client, const std::string& path,
                   time_t timeout) {
  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);

  Davix::DavixError* err = nullptr;
  if (davix_client.rmdir(&params, path, &err)) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return errStatus;
  }

  return XRootDStatus();
}

std::pair<XrdCl::DirectoryList*, XrdCl::XRootDStatus> DirList(
    Davix::DavPosix& davix_client, const std::string& path, bool details,
    bool /*recursive*/, time_t timeout) {
  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);

  auto dir_list = new DirectoryList();

  Davix::DavixError* err = nullptr;

  auto dir_fd = davix_client.opendirpp(&params, SanitizedURL(path), &err);
  if (!dir_fd) {
    auto errStatus = XRootDStatus(stError, errInternal, err->getStatus(),
                                  err->getErrMsg());
    delete err;
    return std::make_pair(nullptr, errStatus);
  }

  struct stat info;
  while (auto entry = davix_client.readdirpp(dir_fd, &info, &err)) {
    if (err) {
      auto errStatus = XRootDStatus(stError, errInternal, err->getStatus(),
                                    err->getErrMsg());
      delete err;
      return std::make_pair(nullptr, errStatus);
    }

    StatInfo* stat_info = nullptr;
    if (details) {
      stat_info = new StatInfo();
      auto res = FillStatInfo(info, stat_info);
      if (res.IsError()) {
        delete entry;
        delete stat_info;
        return std::make_pair(nullptr, res);
      }
    }

    auto list_entry = new DirectoryList::ListEntry(path, entry->d_name, stat_info);
    dir_list->Add(list_entry);

    // do not delete "entry". davix_client.readdirpp() always return the same address
    // and will set it to NULL when there is no more directory entry to return 
    //delete entry;
  }

  if (davix_client.closedirpp(dir_fd, &err)) {
    auto errStatus = XRootDStatus(stError, errInternal, err->getStatus(),
                                  err->getErrMsg());
    delete err;
    return std::make_pair(nullptr, errStatus);
  }

  return std::make_pair(dir_list, XRootDStatus());
}

XRootDStatus Rename(Davix::DavPosix& davix_client, const std::string& source,
                    const std::string& dest, time_t timeout) {

  // most s3 storage systems either:
  // 1. do not support rename, especially for files that were uploaded using multi-part
  // 2. support by copy-n-delete.
  // we could implement copy-n-delete if necessary
  if (getenv("AWS_ACCESS_KEY_ID"))
      return XRootDStatus(stError, errErrorResponse, kXR_Unsupported);

  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);

  Davix::DavixError* err = nullptr;
  if (davix_client.rename(&params, SanitizedURL(source), SanitizedURL(dest), &err)) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return errStatus;
  }

  return XRootDStatus();
}

XRootDStatus Stat(Davix::DavPosix& davix_client, const std::string& url,
                  time_t timeout, StatInfo* stat_info) {
  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);

  struct stat stats;
  Davix::DavixError* err = nullptr;
  if (davix_client.stat(&params, SanitizedURL(url), &stats, &err)) {
    auto res = ErrCodeConvert(err->getStatus());
    auto errStatus =
        XRootDStatus(stError, res.first, res.second, err->getErrMsg());
    delete err;
    return errStatus;
  }

  auto res = FillStatInfo(stats, stat_info);
  if (res.IsError()) {
    return res;
  }

  return XRootDStatus();
}

XRootDStatus Unlink(Davix::DavPosix& davix_client, const std::string& url,
                    time_t timeout) {
  Davix::RequestParams params;
  SetTimeout(params, timeout);
  SetAuthz(params);

  Davix::DavixError* err = nullptr;
  if (davix_client.unlink(&params, SanitizedURL(url), &err)) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return errStatus;
  }

  return XRootDStatus();
}

std::pair<int, XRootDStatus> _PRead(Davix::DavPosix& davix_client, DAVIX_FD* fd,
                                    void* buffer, uint32_t size,
                                    uint64_t offset, bool no_pread = false) {
  Davix::DavixError* err = nullptr;
  int num_bytes_read;
  if (no_pread) { // continue reading from the current offset position
    num_bytes_read = davix_client.read(fd, buffer, size, &err); 
  }
  else {
    num_bytes_read = davix_client.pread(fd, buffer, size, offset, &err);
  }
  if (num_bytes_read < 0) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return std::make_pair(num_bytes_read, errStatus);
  }

  return std::make_pair(num_bytes_read, XRootDStatus());
}

std::pair<int, XRootDStatus> Read(Davix::DavPosix& davix_client, DAVIX_FD* fd,
                                  void* buffer, uint32_t size) {
  return _PRead(davix_client, fd, buffer, size, 0, true);
}

std::pair<int, XRootDStatus> PRead(Davix::DavPosix& davix_client, DAVIX_FD* fd,
                                   void* buffer, uint32_t size, uint64_t offset) {
  return _PRead(davix_client, fd, buffer, size, offset, false);
}

std::pair<int, XrdCl::XRootDStatus> PReadVec(Davix::DavPosix& davix_client,
                                             DAVIX_FD* fd,
                                             const XrdCl::ChunkList& chunks,
                                             void* buffer) {
  const auto num_chunks = chunks.size();
  std::vector<Davix::DavIOVecInput> input_vector(num_chunks);
  std::vector<Davix::DavIOVecOuput> output_vector(num_chunks);

  for (size_t i = 0; i < num_chunks; ++i) {
    input_vector[i].diov_offset = chunks[i].offset;
    input_vector[i].diov_size = chunks[i].length;
    input_vector[i].diov_buffer = chunks[i].buffer;
  }

  Davix::DavixError* err = nullptr;
  int num_bytes_read = davix_client.preadVec(
      fd, input_vector.data(), output_vector.data(), num_chunks, &err);
  if (num_bytes_read < 0) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return std::make_pair(num_bytes_read, XRootDStatus(stError, errUnknown));
  }

  return std::make_pair(num_bytes_read, XRootDStatus());
}

std::pair<int, XrdCl::XRootDStatus> PWrite(Davix::DavPosix& davix_client,
                                           DAVIX_FD* fd, uint64_t offset,
                                           uint32_t size, const void* buffer,
                                           time_t timeout) {
  Davix::DavixError* err = nullptr;
  off_t new_offset = davix_client.lseek(fd, offset, SEEK_SET, &err);
  if (uint64_t(new_offset) != offset) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return std::make_pair(new_offset, errStatus);
  }
  int num_bytes_written = davix_client.write(fd, buffer, size, &err);
  if (num_bytes_written < 0) {
    auto errStatus =
        XRootDStatus(stError, errInternal, err->getStatus(), err->getErrMsg());
    delete err;
    return std::make_pair(num_bytes_written, errStatus);
  }

  return std::make_pair(num_bytes_written, XRootDStatus());
}

}  // namespace Posix

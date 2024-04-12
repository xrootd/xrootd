/**
 * This file is part of XrdClHttp
 */

#ifndef __HTTP_FILE_SYSTEM_PLUG_IN_
#define __HTTP_FILE_SYSTEM_PLUG_IN_

#include "davix.hpp"

#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClURL.hh"

#include <unordered_map>

namespace XrdCl {
class Log;

class HttpFileSystemPlugIn : public FileSystemPlugIn {
 public:
  HttpFileSystemPlugIn(const std::string &url);
  virtual ~HttpFileSystemPlugIn() noexcept;

  virtual XRootDStatus Mv(const std::string &source, const std::string &dest,
                          ResponseHandler *handler, time_t timeout) override;

  virtual XRootDStatus Rm(const std::string &path, ResponseHandler *handler,
                          time_t timeout) override;

  virtual XRootDStatus MkDir(const std::string &path, MkDirFlags::Flags flags,
                             Access::Mode mode, ResponseHandler *handler,
                             time_t timeout) override;

  virtual XRootDStatus RmDir(const std::string &path, ResponseHandler *handler,
                             time_t timeout) override;

  virtual XRootDStatus DirList(const std::string &path,
                               DirListFlags::Flags flags,
                               ResponseHandler *handler,
                               time_t timeout) override;

  virtual XRootDStatus Stat(const std::string &path, ResponseHandler *handler,
                            time_t timeout) override;

  virtual bool SetProperty(const std::string &name,
                           const std::string &value) override;

  virtual bool GetProperty(const std::string &name,
                           std::string &value) const override;

 private:
  Davix::Context *ctx_;
  Davix::DavPosix *davix_client_;

  URL url_;

  std::unordered_map<std::string, std::string> properties_;

  Log *logger_;
};
}  // namespace XrdCl

#endif  // __HTTP_FILE_SYSTEM_PLUG_IN_

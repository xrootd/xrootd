/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

//
// An OSS meant for unit tests.
// Currently, it delays the open of /test/slow_open.txt by 12s.
// Otherwise, requests pass through unmodified.
//

#include "XrdOss/XrdOssWrapper.hh"
#include "XrdVersion.hh"

#include <memory>
#include <string>
#include <unistd.h>

namespace {

class File final : public XrdOssWrapDF {
  public:
    File(std::unique_ptr<XrdOssDF> wrapDF)
        : XrdOssWrapDF(*wrapDF), m_wrapped(std::move(wrapDF)) {}

    virtual ~File() {}

    virtual int Open(const char *path, int Oflag, mode_t Mode,
             XrdOucEnv &env) override {
        m_path = std::string(path);
        if (m_path == "/test/slow_open.txt") {
            usleep(12'000'000); // 12s
        }
        else if (m_path == "/test/slow_read.txt" || m_path == "/test/stall_read.txt") {
            return 0;
        }

        return wrapDF.Open(path, Oflag, Mode, env);
    }

    virtual ssize_t Read(void *buffer, off_t offset, size_t size) override {
        if (m_path == "/test/stall_read.txt") {
            if (offset > 3 * 1024 * 1024) {
                usleep(12'000'000); // 12s
            }

            std::string a_repeated = std::string(size, 'a');
            memcpy(buffer, a_repeated.data(), size);
            return size;
        } else if (m_path == "/test/slow_read.txt") {
            usleep(500'000); // 1ms
            auto xfer = 256 < size ? 256 : size;
            std::string a_repeated = std::string(xfer, 'a');
            memcpy(buffer, a_repeated.data(), xfer);
            return xfer;
        } else {
            return wrapDF.Read(buffer, offset, size);
        }
    }

    virtual int Fstat(struct stat *buff) override {
        if (buff && (m_path == "/test/slow_read.txt" || m_path == "/test/stall_read.txt")) {
            memset(buff, 0, sizeof(struct stat));
            buff->st_mode = S_IFREG | 0644;
            buff->st_size = 1024 * 1024 * 1024; // 1GB
            buff->st_nlink = 1;
            return 0;
        }
        return wrapDF.Fstat(buff);
    }

  private:
    std::unique_ptr<XrdOssDF> m_wrapped;
    std::string m_path;
};

class FileSystem final : public XrdOssWrapper {
  public:
    FileSystem(XrdOss *oss, XrdSysLogger *log, XrdOucEnv *envP)
        : XrdOssWrapper(*oss), m_oss(oss) {}

    virtual ~FileSystem() {}

    virtual XrdOssDF *newFile(const char *user = 0) override {
        std::unique_ptr<XrdOssDF> wrapped(wrapPI.newFile(user));
        return new File(std::move(wrapped));
    }

    virtual int Stat(const char *path, struct stat *buff, int opts = 0,
                         XrdOucEnv *env = 0) override {
        fprintf(stderr, "Got stat for path: %s\n", path);
        if (!strcmp(path, "/test/slow_open.txt")) {
            usleep(12'000'000); // 12s
        } else if (buff && (!strcmp(path, "/test/slow_read.txt") || !strcmp(path, "/test/stall_read.txt"))) {
            memset(buff, 0, sizeof(struct stat));
            buff->st_mode = S_IFREG | 0644;
            buff->st_size = 1024 * 1024 * 1024; // 1GB
            buff->st_nlink = 1;
            return 0;
        }
        return wrapPI.Stat(path, buff, opts, env);
    }

  private:
    std::unique_ptr<XrdOss> m_oss;
};

} // namespace

extern "C" {

XrdOss *XrdOssAddStorageSystem2(XrdOss *curr_oss, XrdSysLogger *logger,
                                const char *config_fn, const char *parms,
                                XrdOucEnv *envP) {
    return new FileSystem(curr_oss, logger, envP);
}

XrdVERSIONINFO(XrdOssAddStorageSystem2, slowfs);

} // extern "C"

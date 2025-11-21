
//
// An OSS meant for unit tests.
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

    int Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env) override {
        std::string path_str(path);
        auto const pos = path_str.find_last_of('/');
        const auto leaf = path_str.substr(pos + 1);

        if (leaf == "no_space.txt") {
            errorCode = ENOSPC;
            m_write_fail_with_offset = true;
        } else if (leaf == "fail_read.txt") {
            errorCode = EIO;
            m_read_fail_with_offset = true;
        } else if (leaf == "out_of_space_quota.txt") {
            errorCode = EDQUOT;
            m_write_fail_with_offset = true;
        } else if (leaf == "file_does_not_exist") {
            return -ENOENT;
        } else if (leaf == "unreadable_file") {
            errorCode = EBADF;
            m_read_fail = true;
        }
        return wrapDF.Open(path, Oflag, Mode, env);
    }

    ssize_t Read(void *buffer, off_t offset, size_t size) override {
        if (m_read_fail_with_offset && offset > 0) return -errorCode;
        if (errorCode > 0 && m_read_fail) return -errorCode;

        return wrapDF.Read(buffer, offset, size);
    }

    ssize_t Write(const void *buffer, off_t offset, size_t size) override {
        if (m_write_fail_with_offset && offset > 0) return -errorCode;
        if (errorCode >= 0 && m_write_fail) return -errorCode;

        return wrapDF.Write(buffer, offset, size);
    }

    int getFD() override {return -1;}

  private:
    bool m_read_fail{false}; //fail on initial read 
    bool m_read_fail_with_offset{false}; //fail for subsequent read chunks
    bool m_write_fail{false}; // fail on initial write 
    bool m_write_fail_with_offset{false}; //fail for subsequent write chunks
    int errorCode{-1}; //error code to return on failure
    std::unique_ptr<XrdOssDF> m_wrapped;
};

class FileSystem final : public XrdOssWrapper {
  public:
    FileSystem(XrdOss *oss, XrdSysLogger *log, XrdOucEnv *envP)
        : XrdOssWrapper(*oss), m_oss(oss) {}

    virtual ~FileSystem() {}

    XrdOssDF *newFile(const char *user = 0) override {
        std::unique_ptr<XrdOssDF> wrapped(wrapPI.newFile(user));
        return new File(std::move(wrapped));
    }

    int Create(const char *tid, const char *path, mode_t mode,
      XrdOucEnv &env, int opts=0) override {
        std::string path_str(path);
        auto const pos = path_str.find_last_of('/');
        const auto filename = path_str.substr(pos + 1);

        if (filename == "no_inode.txt" ) return -ENOSPC;
        else if (filename == "out_of_inode_quota.txt") return -EDQUOT;
        return wrapPI.Create(tid, path, mode, env, opts);
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

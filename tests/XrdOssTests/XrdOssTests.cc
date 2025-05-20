
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
        m_fail = leaf == "fail_read.txt";
        return wrapDF.Open(path, Oflag, Mode, env);
    }

    ssize_t Read(void *buffer, off_t offset, size_t size) override {
        if (m_fail && offset > 0) return -EIO;
        return wrapDF.Read(buffer, offset, size);
    }

    int getFD() override {return -1;}

  private:
    bool m_fail{false};
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


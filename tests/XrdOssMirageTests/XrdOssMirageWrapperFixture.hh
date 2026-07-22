#ifndef __XRD_OSS_MIRAGE_WRAPPER_FIXTURE_HH__
#define __XRD_OSS_MIRAGE_WRAPPER_FIXTURE_HH__

#include "XrdOssMirage/XrdOssMirageWrapper.hh"

#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

//
// The prefix the wrapper is configured with throughout the tests. It matches
// the compiled-in default, so the process-wide immutable prefix resolves to
// the same value regardless of which test happens to lock it in first.
//
inline constexpr const char *WRAPPER_PREFIX = "/mirage/";

//
// Distinctive value returned by the mocked stacked plugin. Any routed call
// that comes back with this value was handled by the stacked plugin rather
// than by XrdOssMirage.
//
inline constexpr int STACKED_RC = 0xC0DE;

//
// Mock of the stacked plugin's file/directory object.
//
class MockOssDF : public XrdOssDF {
public:
    MOCK_METHOD(int,     Open,      (const char *path, int Oflag, mode_t Mode, XrdOucEnv &env), (override));
    MOCK_METHOD(int,     Opendir,   (const char *path, XrdOucEnv &env), (override));
    MOCK_METHOD(ssize_t, Read,      (void *buffer, off_t offset, size_t size), (override));
    MOCK_METHOD(ssize_t, Write,     (const void *buffer, off_t offset, size_t size), (override));
    MOCK_METHOD(int,     Fstat,     (struct stat *buf), (override));
    MOCK_METHOD(int,     Ftruncate, (unsigned long long flen), (override));
    MOCK_METHOD(int,     Readdir,   (char *buff, int blen), (override));
    MOCK_METHOD(int,     Close,     (long long *retsz), (override));
    MOCK_METHOD(int,     getFD,     (), (override));
};

//
// Mock of the stacked plugin's storage system. newDir()/newFile() hand out
// mock directory/file objects and remember the most recent one so tests can
// set expectations on it.
//
class MockOss : public XrdOss {
public:
    MOCK_METHOD(int, Chmod,    (const char *path, mode_t mode, XrdOucEnv *envP), (override));
    MOCK_METHOD(int, Create,   (const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts), (override));
    MOCK_METHOD(int, Init,     (XrdSysLogger *lp, const char *cfn), (override));
    MOCK_METHOD(int, Mkdir,    (const char *path, mode_t mode, int mkpath, XrdOucEnv *envP), (override));
    MOCK_METHOD(int, Remdir,   (const char *path, int Opts, XrdOucEnv *envP), (override));
    MOCK_METHOD(int, Rename,   (const char *oPath, const char *nPath, XrdOucEnv *oEnvP, XrdOucEnv *nEnvP), (override));
    MOCK_METHOD(int, Stat,     (const char *path, struct stat *buff, int opts, XrdOucEnv *envP), (override));
    MOCK_METHOD(int, Truncate, (const char *path, unsigned long long fsize, XrdOucEnv *envP), (override));
    MOCK_METHOD(int, Unlink,   (const char *path, int Opts, XrdOucEnv *envP), (override));

    MOCK_METHOD(XrdOssDF *, newDir,  (const char *tident), (override));
    MOCK_METHOD(XrdOssDF *, newFile, (const char *tident), (override));
};

//
// Wraps an XrdOssMirageWrapper in front of the mocked stacked plugin.
// The wrapper owns the mock OSS; mock_oss/mock_df stay valid for as long as the
// owning objects live.
//
class XrdOssMirageWrapperFixture : public testing::Test {
protected:
    XrdOssMirageWrapperFixture()
        : wrapper(&mock_oss, nullptr, WRAPPER_PREFIX, nullptr)
    {
        using testing::_;
        using testing::Invoke;

        // newDir()/newFile() return a fresh mock object and record it so the
        // tests can express expectations on the directory/file actually used.
        ON_CALL(mock_oss, newDir(_)).WillByDefault(Invoke([this](const char *) {
            return mock_df = new testing::NiceMock<MockOssDF>();
        }));
        ON_CALL(mock_oss, newFile(_)).WillByDefault(Invoke([this](const char *) {
            return mock_df = new testing::NiceMock<MockOssDF>();
        }));
    }

    XrdOucEnv                       env;
    testing::NiceMock<MockOss>     mock_oss;
    XrdOssMirageWrapper   wrapper;
    testing::NiceMock<MockOssDF>   *mock_df = nullptr; // owned by the wrapper's File/Dir
};

#endif

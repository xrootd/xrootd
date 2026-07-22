#ifndef __XRD_OSS_MIRAGE_XATTR_WRAPPER_FIXTURE_HH__
#define __XRD_OSS_MIRAGE_XATTR_WRAPPER_FIXTURE_HH__

#include "XrdOssMirage/XrdOssMirage.hh"
#include "XrdOssMirage/XrdOssMirageXAttrWrapper.hh"

#include "XrdOuc/XrdOucEnv.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

//
// The prefix the wrapper is configured with throughout the tests. It matches
// the compiled-in default, so the process-wide immutable prefix resolves to
// the same value regardless of which test happens to lock it in first.
//
inline constexpr const char *XATTR_WRAPPER_PREFIX = "/mirage/";

//
// Distinctive value returned by the mocked wrapped plugin. Any routed call
// that comes back with this value was handled by the wrapped plugin rather
// than by XrdOssMirageXAttr.
//
inline constexpr int WRAPPED_RC = 0xC0DE;

//
// Mock of the wrapped extended-attribute plugin.
//
class MockXAttr : public XrdSysXAttr {
public:
    MOCK_METHOD(int,  Del,  (const char *Aname, const char *Path, int fd), (override));
    MOCK_METHOD(void, Free, (AList *aPL), (override));
    MOCK_METHOD(int,  Get,  (const char *Aname, void *Aval, int Avsz, const char *Path, int fd), (override));
    MOCK_METHOD(int,  List, (AList **aPL, const char *Path, int fd, int getSz), (override));
    MOCK_METHOD(int,  Set,  (const char *Aname, const void *Aval, int Avsz, const char *Path, int fd, int isNew), (override));
};

//
// Wraps an XrdOssMirageXAttrWrapper in front of the mocked wrapped plugin and
// connects it to a local XrdOssMirage that already holds a "/mirage/dummy"
// entry. Attribute operations under the prefix are served by XrdOssMirage;
// everything else is forwarded to the mock.
//
class XrdOssMirageXAttrWrapperFixture : public testing::Test {
protected:
    XrdOssMirageXAttrWrapperFixture()
        : wrapper(nullptr, nullptr, XATTR_WRAPPER_PREFIX, nullptr, &mock_xattr)
    {
        wrapper.setOss(oss);
    }

    void SetUp() override
    {
        oss.Create(nullptr, "/mirage/dummy", {}, env, XRDOSS_new);
        oss.Truncate("/mirage/dummy", 9999);
    }

    XrdOucEnv                     env;
    XrdOssMirage                  oss;
    testing::NiceMock<MockXAttr>  mock_xattr;
    XrdOssMirageXAttrWrapper      wrapper;
};

#endif

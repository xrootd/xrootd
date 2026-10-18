#ifndef __XRD_OSS_MIRAGE_FIXTURE_HH__
#define __XRD_OSS_MIRAGE_FIXTURE_HH__

#include "XrdOssMirage/XrdOssMirage.hh"
#include "XrdOssMirage/XrdOssMirageFile.hh"
#include "XrdOssMirage/XrdOssMirageXAttr.hh"

#include "XrdOuc/XrdOucEnv.hh"

#include <gtest/gtest.h>

class XrdOssMirageFixture : public testing::Test {
protected:
    XrdOssMirageFixture() : file(oss)
    {
        xattr.setOss(oss);
    }

    void SetUp() override
    {
      oss.Create(nullptr, "/dummy", {}, env, XRDOSS_new);
      oss.Truncate("/dummy", 9999);
    }

    XrdOucEnv env;
    XrdOssMirage oss;
    XrdOssMirageFile file;
    XrdOssMirageXAttr xattr;
};

#endif

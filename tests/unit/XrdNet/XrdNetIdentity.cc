//------------------------------------------------------------------------------
// Unit tests for XrdNetIdentity
//------------------------------------------------------------------------------

#include "XrdNet/XrdNetIdentity.hh"

#include <gtest/gtest.h>

// The -N option of xrootd calls SetFQN(). getMyFQN() and XrdNetAddr give
// lowercase names, and InDomain() compares them with strcmp(). Thus SetFQN()
// must also give a lowercase name.
TEST(IdentityTest, SetFQNLowercasesTheName)
{
  XrdNetIdentity::SetFQN("Host.Example.COM");
  EXPECT_STREQ(XrdNetIdentity::FQN(), "host.example.com");
}

TEST(IdentityTest, SetFQNLowercasesTheDomain)
{
  XrdNetIdentity::SetFQN("Host.Example.COM");
  EXPECT_STREQ(XrdNetIdentity::Domain(), ".example.com");
}

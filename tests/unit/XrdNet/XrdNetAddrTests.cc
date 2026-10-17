//------------------------------------------------------------------------------
// Unit tests for XrdNetAddr
//------------------------------------------------------------------------------

#include "XrdNet/XrdNetAddr.hh"

#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>

namespace {

// The size of the socket address for its family
size_t FamilySize(XrdNetAddr &addr)
{
  return addr.SockAddr()->sa_family == AF_INET ? sizeof(sockaddr_in)
                                               : sizeof(sockaddr_in6);
}

// The port makes "127.0.0.1:1094" look like a host name, so Set() resolves it
// with getaddrinfo(). Without SetIPV4() or SetIPV6(), the resolver can give an
// IPv4 address. Its size must not be the size of an IPv6 address, as macOS
// then rejects it in connect() and bind() with EINVAL.
TEST(XrdNetAddr, AddressWithAPortHasTheSizeOfItsFamily)
{
  XrdNetAddr addr;
  ASSERT_EQ(addr.Set("127.0.0.1:1094"), nullptr);
  EXPECT_EQ(addr.SockSize(), FamilySize(addr));
}

// The same applies to a host name
TEST(XrdNetAddr, HostNameHasTheSizeOfItsFamily)
{
  XrdNetAddr addr;
  ASSERT_EQ(addr.Set("localhost:1094"), nullptr);
  EXPECT_EQ(addr.SockSize(), FamilySize(addr));
}

} // namespace

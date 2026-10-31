//------------------------------------------------------------------------------
// Unit tests for XrdNetMsg
//------------------------------------------------------------------------------

#include "XrdNet/XrdNetMsg.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

// The largest UDP payload over IPv4: 65535 bytes less the 20-byte IP header
// and the 8-byte UDP header
const size_t maxPayload = 65507;

//------------------------------------------------------------------------------
//! A loopback UDP socket that receives one datagram
//------------------------------------------------------------------------------

class Receiver
{
public:
  Receiver()
  {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
      return;
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    auto *sap = reinterpret_cast<sockaddr *>(&sa);
    socklen_t slen = sizeof(sa);
    if (bind(fd, sap, sizeof(sa)) != 0 || getsockname(fd, sap, &slen) != 0)
      return;
    port = ntohs(sa.sin_port);
    int rcvbuf = 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    timeval tv = {1, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  ~Receiver()
  {
    if (fd >= 0)
      close(fd);
  }

  // The size of the next datagram, or -1 if none arrives
  ssize_t Next()
  {
    std::vector<char> buff(65536);
    return recv(fd, buff.data(), buff.size(), 0);
  }

  int fd = -1;
  int port = 0;
};

// XrdNet gives a UDP socket a 32 KiB send buffer. On some systems, such as
// macOS, a larger datagram then fails with EMSGSIZE. A monitor buffer can be
// as large as the largest UDP payload, so XrdNetMsg must send it.
TEST(XrdNetMsg, SendsTheLargestUdpPayload)
{
  Receiver rcv;
  ASSERT_GT(rcv.port, 0) << "no loopback UDP socket";

  XrdSysLogger logger;
  XrdSysError eDest(&logger, "nettest");
  char dest[64];
  snprintf(dest, sizeof(dest), "127.0.0.1:%d", rcv.port);
  bool aOK = false;
  XrdNetMsg msg(&eDest, dest, &aOK);
  ASSERT_TRUE(aOK) << "XrdNetMsg could not connect to " << dest;

  const std::string payload(maxPayload, 'x');
  ASSERT_EQ(msg.Send(payload.data(), payload.size()), 0);
  EXPECT_EQ(rcv.Next(), static_cast<ssize_t>(maxPayload));
}

} // namespace

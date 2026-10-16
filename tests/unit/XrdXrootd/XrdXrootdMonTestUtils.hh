#ifndef XRDXROOTDMONTESTUTILS_HH
#define XRDXROOTDMONTESTUTILS_HH
//------------------------------------------------------------------------------
// Shared helpers for the monitoring unit tests.
//
// The tests start the real monitor with a loopback UDP destination and examine
// the datagrams that come out, which is what a collector sees.
//
// The scheduler given to the monitor is never started. So no timed flush
// occurs: a buffer is sent when it is full, or when a test calls Flush(). This
// keeps the sequence of datagrams deterministic.
//
// The monitor statics are global to the process and Init() runs only once, so
// each set of monitor options needs its own test executable.
//------------------------------------------------------------------------------

#include "Xrd/XrdScheduler.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"

#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace XrdXrootdMonTest
{

//------------------------------------------------------------------------------
//! A loopback UDP receiver for the monitoring datagrams
//------------------------------------------------------------------------------

class Receiver
{
public:
  bool Open()
  {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
      return false;

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;
    auto *sap = reinterpret_cast<sockaddr *>(&sa);
    if (bind(fd, sap, sizeof(sa)) != 0)
      return false;

    socklen_t slen = sizeof(sa);
    if (getsockname(fd, sap, &slen) != 0)
      return false;
    port = ntohs(sa.sin_port);

    // A large receive buffer, so that no datagram is lost, and a short
    // timeout, so that Drain() stops when no more datagrams arrive.
    int rcvbuf = 4 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    timeval tv = {0, 200 * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return true;
  }

  // Read all datagrams that arrived
  std::vector<std::string> Drain()
  {
    std::vector<std::string> out;
    std::vector<char> buff(65536);
    for (;;) {
      ssize_t n = recv(fd, buff.data(), buff.size(), 0);
      if (n <= 0)
        break;
      out.emplace_back(buff.data(), static_cast<size_t>(n));
    }
    return out;
  }

  ~Receiver()
  {
    if (fd >= 0)
      close(fd);
  }

  int fd = -1;
  int port = 0;
};

//------------------------------------------------------------------------------
//! The monitor options of one test executable
//------------------------------------------------------------------------------

struct MonConfig
{
  int fsOpts = 0;                                        // fstat options
  int fstatBsz = 1024;                                   // fbsz
  int monBsz = 16384;                                    // mbuff
  int destMode = XROOTD_MON_FSTA | XROOTD_MON_FILE;      // what the dest gets
};

//------------------------------------------------------------------------------
//! Start the monitor once for all tests in an executable
//------------------------------------------------------------------------------

class MonEnv : public ::testing::Environment
{
public:
  explicit MonEnv(const MonConfig &config) : cfg(config) {}

  explicit MonEnv(int fsOpts) : cfg{fsOpts} {}

  void SetUp() override
  {
    udpOK = rcv.Open();
    if (!udpOK)
      return;

    // This call sets monFSTAT and gives the fstat options and buffer size to
    // XrdXrootdMonFile::Defaults(). The destination call below must come
    // after it.
    XrdXrootdMonitor::Defaults(cfg.monBsz, 32768, 60, // mbuff, rbuff, window
                               1, 0,                  // flush, flash
                               -1,                    // ident off
                               1,                     // rnums
                               cfg.fstatBsz,          // fbsz
                               1,                     // fstat interval
                               cfg.fsOpts,
                               1);                    // xfr count

    char dest[64];
    snprintf(dest, sizeof(dest), "127.0.0.1:%d", rcv.port);
    XrdXrootdMonitor::Defaults(strdup(dest), cfg.destMode, nullptr, 0);

    XrdXrootdMonitor::Init(&sched, &eDest, "testhost", "xrootd", "test", 1094);
    started = XrdXrootdMonitor::Init() != 0;
  }

  MonConfig cfg;
  XrdSysLogger logger{STDERR_FILENO, 0}; // shows why Init() fails
  XrdSysError eDest{&logger, "montest"};
  XrdScheduler sched;
  Receiver rcv;
  bool udpOK = false;
  bool started = false;
};

// The environment of the test executable. Its main() sets it.
inline MonEnv *env = nullptr;

//------------------------------------------------------------------------------
//! The base of the monitor tests. A system without loopback UDP cannot run
//! them, so they skip. But a monitor that does not start is a failure.
//------------------------------------------------------------------------------

class MonTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    ASSERT_NE(env, nullptr);
    if (!env->udpOK)
      GTEST_SKIP() << "no loopback UDP socket";
    ASSERT_TRUE(env->started) << "XrdXrootdMonitor::Init() failed";
  }
};

//------------------------------------------------------------------------------
//! Send the records in the f-stream buffer now
//------------------------------------------------------------------------------

inline void Flush()
{
  static XrdXrootdMonFile job;
  job.DoIt();
}

//------------------------------------------------------------------------------
// Record access, as a collector does it
//------------------------------------------------------------------------------

struct Record
{
  unsigned char type;
  unsigned char flag;
  std::string data; // the full record, header included
};

// The code of a datagram, from its header
inline char Code(const std::string &dgram)
{
  XrdXrootdMonHeader h;
  if (dgram.size() < sizeof(h))
    return 0;
  memcpy(&h, dgram.data(), sizeof(h));
  return static_cast<char>(h.code);
}

// Keep only the datagrams with the given code
inline std::vector<std::string> Stream(const std::vector<std::string> &dgrams,
                                       char code)
{
  std::vector<std::string> out;
  for (const auto &d : dgrams)
    if (Code(d) == code)
      out.push_back(d);
  return out;
}

// Keep only the f-stream datagrams
inline std::vector<std::string> FStream(const std::vector<std::string> &dgrams)
{
  return Stream(dgrams, XROOTD_MON_MAPFSTA);
}

// Get the records of one f-stream datagram. Return false if the records do not
// fill the datagram exactly. A collector rejects such a datagram.
inline bool WalkRecords(const std::string &dgram, std::vector<Record> &recs)
{
  const size_t len = dgram.size();
  size_t off = sizeof(XrdXrootdMonHeader);

  if (len < off)
    return false;

  while (off < len) {
    XrdXrootdMonFileHdr h;
    if (len - off < sizeof(h))
      return false;
    memcpy(&h, dgram.data() + off, sizeof(h));
    const size_t rsz = ntohs(h.recSize);
    if (rsz < sizeof(h) || off + rsz > len)
      return false;
    recs.push_back({static_cast<unsigned char>(h.recType),
                    static_cast<unsigned char>(h.recFlag),
                    dgram.substr(off, rsz)});
    off += rsz;
  }
  return true;
}

// Get all f-stream records. A datagram that the records do not fill exactly
// fails the test.
inline std::vector<Record> Records(const std::vector<std::string> &dgrams)
{
  std::vector<Record> out;
  for (const auto &d : FStream(dgrams))
    EXPECT_TRUE(WalkRecords(d, out)) << "records do not fill the datagram";
  return out;
}

// Get all f-stream records of a given type
inline std::vector<Record> Records(const std::vector<std::string> &dgrams,
                                   int type)
{
  std::vector<Record> out;
  for (auto &r : Records(dgrams))
    if (r.type == type)
      out.push_back(std::move(r));
  return out;
}

// The file id of an f-stream record, as the monitor wrote it
inline kXR_unt32 FileID(const Record &r)
{
  XrdXrootdMonFileHdr h;
  memcpy(&h, r.data.data(), sizeof(h));
  return h.fileID;
}

} // namespace XrdXrootdMonTest

#endif

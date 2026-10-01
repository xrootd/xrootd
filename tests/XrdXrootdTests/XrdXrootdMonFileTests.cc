//------------------------------------------------------------------------------
// Unit tests for the f-stream (fstat) monitoring buffer in XrdXrootdMonFile.
//
// The buffer is a single posix_memalign'd region carved into variable-length
// records by the private GetSlot(). Every piece of its state is a private
// static with no accessor, so these tests do not inspect it. They stand up the
// real monitor against a loopback UDP socket and assert on the datagrams that
// come out, which is the behaviour that actually reaches a collector:
//
//   - no datagram exceeds the configured buffer, even for an open whose path is
//     far longer than the buffer (the heap overflow these tests guard);
//   - the records tile each datagram exactly, walking by recSize, which is what
//     XrdMonDecode::DecodeFStream does and what a bogus recSize would break;
//   - an open record never exceeds XrdXrootdMonFileOPN, the structure that
//     collectors are compiled against, and its lfn is null-terminated;
//   - close records are emitted even though the "xfr" sub-option is off.
//
// The scheduler passed to the monitor is never Start()ed. Schedule() only
// inserts into the timer queue, so no job ever runs and flushes happen solely
// because the buffer filled up. That keeps the datagram stream deterministic.
//
// The monitor's statics are process-wide and Init() allocates the buffer once,
// so this file must be its own test binary.
//------------------------------------------------------------------------------

#include "Xrd/XrdScheduler.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"

#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// The buffer we ask for. Defaults() raises it to fbszMin, so this also
// exercises the floor.
//
const int askedBsz = 1024;

// A path far longer than the buffer. Open() must clamp it rather than run off
// the end of the buffer.
//
const int longPathLen = 4000;

//------------------------------------------------------------------------------
//! A loopback UDP receiver. Binds an ephemeral port on 127.0.0.1 and reads
//! whatever the monitor sends, without blocking once the stream runs dry.
//------------------------------------------------------------------------------

class Receiver
{
public:

bool Open()
    {fd = socket(AF_INET, SOCK_DGRAM, 0);
     if (fd < 0) return false;

     sockaddr_in sa;
     memset(&sa, 0, sizeof(sa));
     sa.sin_family = AF_INET;
     sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
     sa.sin_port = 0;
     if (bind(fd, (sockaddr *)&sa, sizeof(sa))) return false;

     socklen_t slen = sizeof(sa);
     if (getsockname(fd, (sockaddr *)&sa, &slen)) return false;
     port = ntohs(sa.sin_port);

     // A large receive buffer so nothing is lost while the test drives the
     // monitor, and a short timeout so Drain() ends on its own.
     //
     int rcvbuf = 4*1024*1024;
     setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
     timeval tv = {0, 200*1000};
     setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
     return true;
    }

// Read every datagram that has arrived. The receive timeout ends the loop.
//
std::vector<std::string> Drain()
    {std::vector<std::string> out;
     char buff[65536];
     for (;;)
         {ssize_t n = recv(fd, buff, sizeof(buff), 0);
          if (n <= 0) break;
          out.push_back(std::string(buff, (size_t)n));
         }
     return out;
    }

    ~Receiver() {if (fd >= 0) close(fd);}

int fd   = -1;
int port = 0;
};

//------------------------------------------------------------------------------
//! One-time monitor stand-up, shared by every test in this binary.
//------------------------------------------------------------------------------

class MonFileEnv : public ::testing::Environment
{
public:

void SetUp() override
    {if (!rcv.Open()) return;

     // Order matters. This overload seeds monFSTAT and hands the fstat options
     // and buffer size to XrdXrootdMonFile::Defaults(); the destination
     // overload below then ANDs monFSTAT with the configured modes.
     //
     // "xfr" is deliberately absent: a file is then never entered in the
     // transfer map, which used to suppress its close record entirely.
     //
     XrdXrootdMonitor::Defaults(16384, 32768, 60,   // mbuff, rbuff, window
                                1, 0,               // flush, flash
                                -1,                 // ident off
                                1,                  // rnums
                                askedBsz,           // fbsz
                                1,                  // fstat interval
                                XROOTD_MON_FSLFN | XROOTD_MON_FSOPS
                                                  | XROOTD_MON_FSSSQ,
                                1);                 // xfr count (unused)

     char dest[64];
     snprintf(dest, sizeof(dest), "127.0.0.1:%d", rcv.port);
     XrdXrootdMonitor::Defaults(strdup(dest), XROOTD_MON_FSTA, nullptr, 0);

     // The scheduler is never started; see the file comment.
     //
     XrdXrootdMonitor::Init(&sched, &eDest, "testhost", "xrootd", "test", 1094);
     ready = XrdXrootdMonitor::Init() != 0;
    }

XrdSysLogger  logger{-1, 0};
XrdSysError   eDest{&logger, "montest"};
XrdScheduler  sched;
Receiver      rcv;
bool          ready = false;
};

MonFileEnv *env = nullptr;

//------------------------------------------------------------------------------
// Record walking, mirroring what a collector does.
//------------------------------------------------------------------------------

struct Record
{
unsigned char       type;
unsigned char       flag;
int                 size;
const unsigned char *body;   // start of the record, header included
};

// Walk one datagram. Returns false if the records do not tile it exactly, which
// is precisely the condition that makes a collector reject the packet.
//
bool WalkRecords(const std::string &dgram, std::vector<Record> &recs)
{
   const unsigned char *p = (const unsigned char *)dgram.data();
   int len = (int)dgram.size();
   int off = (int)sizeof(XrdXrootdMonHeader);

   if (len < off) return false;

   while(off < len)
        {if (len - off < (int)sizeof(XrdXrootdMonFileHdr)) return false;
         const XrdXrootdMonFileHdr *h = (const XrdXrootdMonFileHdr *)(p + off);
         int rsz = ntohs(h->recSize);
         if (rsz < (int)sizeof(XrdXrootdMonFileHdr)) return false;
         if (off + rsz > len) return false;
         recs.push_back({(unsigned char)h->recType, (unsigned char)h->recFlag,
                         rsz, p + off});
         off += rsz;
        }
   return off == len;
}

// Drive the monitor hard enough to force many flushes. Each open record is
// close to the whole buffer, so nearly every call flushes the previous one.
//
void DriveMonitor(int rounds)
{
   std::string path(longPathLen, 'x');
   path[0] = '/';

   for (int i = 0; i < rounds; i++)
       {XrdXrootdFileStats stats;
        stats.Init();
        stats.fSize = 1234;
        XrdXrootdMonFile::Open(&stats, path.c_str(), 0x12345678, i & 1);
        XrdXrootdMonFile::Close(&stats, false);
        XrdXrootdMonFile::Disc(0x87654321);
       }
}

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------

class MonFileTest : public ::testing::Test
{
protected:
void SetUp() override
    {if (!env || !env->ready)
        GTEST_SKIP() << "loopback UDP monitoring could not be set up";
    }
};

// The regression this file exists for. Before the buffer was bounded, an open
// with a path longer than the buffer wrote past the end of it.
//
TEST_F(MonFileTest, DatagramsNeverExceedTheBuffer)
{
   DriveMonitor(40);
   auto dgrams = env->rcv.Drain();

   ASSERT_FALSE(dgrams.empty()) << "no monitoring datagrams arrived";

   for (const auto &d : dgrams)
       {EXPECT_LE((int)d.size(), XrdXrootdMonFile::fbszMin)
            << "datagram larger than the fstat buffer";

        const XrdXrootdMonHeader *h = (const XrdXrootdMonHeader *)d.data();
        EXPECT_EQ(h->code, XROOTD_MON_MAPFSTA);
        EXPECT_EQ((int)ntohs(h->plen), (int)d.size());
       }
}

// A record whose size runs past the datagram is what makes a collector report a
// malformed packet, so pin the tiling.
//
TEST_F(MonFileTest, RecordsTileEachDatagram)
{
   DriveMonitor(40);
   auto dgrams = env->rcv.Drain();

   ASSERT_FALSE(dgrams.empty());

   for (const auto &d : dgrams)
       {std::vector<Record> recs;
        ASSERT_TRUE(WalkRecords(d, recs)) << "records do not tile the datagram";
        ASSERT_FALSE(recs.empty());

        // The first record is always the time record.
        //
        EXPECT_EQ(recs[0].type, XrdXrootdMonFileHdr::isTime);
        EXPECT_EQ(recs[0].size, (int)sizeof(XrdXrootdMonFileTOD));
       }
}

// The emitter must honour the structure collectors are compiled against, and
// the lfn must end in a null even when the path was truncated.
//
TEST_F(MonFileTest, OpenRecordsHonourTheWireStructure)
{
   DriveMonitor(40);
   auto dgrams = env->rcv.Drain();

   ASSERT_FALSE(dgrams.empty());

   int nOpen = 0;
   for (const auto &d : dgrams)
       {std::vector<Record> recs;
        ASSERT_TRUE(WalkRecords(d, recs));

        for (const auto &r : recs)
            {if (r.type != XrdXrootdMonFileHdr::isOpen) continue;
             nOpen++;

             EXPECT_LE(r.size, (int)sizeof(XrdXrootdMonFileOPN))
                 << "open record larger than XrdXrootdMonFileOPN";

             ASSERT_TRUE(r.flag & XrdXrootdMonFileHdr::hasLFN);
             ASSERT_GT(r.size, 20);

             // The lfn starts after the header, the file size and the user
             // dictid. It must be terminated inside the record.
             //
             const char *lfn = (const char *)(r.body + 20);
             int room = r.size - 20;
             EXPECT_LT((int)strnlen(lfn, room), room)
                 << "lfn is not null-terminated within the record";
            }
       }
   EXPECT_GT(nOpen, 0) << "no open records were emitted";
}

// The monitor above is configured without "xfr", so no file is entered in the
// transfer map. Close records must still come out.
//
TEST_F(MonFileTest, CloseRecordsAppearWithoutXfr)
{
   DriveMonitor(40);
   auto dgrams = env->rcv.Drain();

   ASSERT_FALSE(dgrams.empty());

   int nClose = 0;
   for (const auto &d : dgrams)
       {std::vector<Record> recs;
        ASSERT_TRUE(WalkRecords(d, recs));
        for (const auto &r : recs)
            if (r.type == XrdXrootdMonFileHdr::isClose) nClose++;
       }
   EXPECT_GT(nClose, 0) << "no close records were emitted without the xfr option";
}

}

int main(int argc, char **argv)
{
   ::testing::InitGoogleTest(&argc, argv);
   env = new MonFileEnv();
   ::testing::AddGlobalTestEnvironment(env);
   return RUN_ALL_TESTS();
}

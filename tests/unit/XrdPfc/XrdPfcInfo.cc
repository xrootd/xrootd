//------------------------------------------------------------------------------
// Tests for XrdPfc::Info -- the .cinfo file.
//
// The cinfo is read from caches populated by older releases, so its layout and
// its two checksums are a compatibility surface: a change that silently alters
// either turns a populated production cache into a cold one, or worse, into a
// cache that trusts a bit vector it should not. These tests pin down the round
// trip, what happens to damaged files, and the block-accounting arithmetic.
//
// Info reads and writes through an XrdOssDF, and calls nothing on it but
// Read(buf, off, len) and Write(buf, off, len). A vector standing in for the
// file is therefore enough, and it lets a test damage the bytes in ways a real
// filesystem makes awkward.
//------------------------------------------------------------------------------

#include "XrdPfc/XrdPfcInfo.hh"
#include "XrdPfc/XrdPfcStats.hh"

#include "XrdOss/XrdOss.hh"
#include "XrdSys/XrdSysTrace.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

using namespace XrdPfc;

namespace
{

//------------------------------------------------------------------------------
// An XrdOssDF backed by a byte vector.
//------------------------------------------------------------------------------

class MemFile : public XrdOssDF
{
public:
   std::vector<char> m_bytes;

   ssize_t Read(void *buffer, off_t offset, size_t size)
   {
      if (offset < 0) return -EINVAL;
      if ((size_t) offset >= m_bytes.size()) return 0;
      size_t n = std::min(size, m_bytes.size() - (size_t) offset);
      memcpy(buffer, m_bytes.data() + offset, n);
      return (ssize_t) n;
   }

   // The only pure virtual in XrdOssDF; Info never calls it.
   int Close(long long *retsz = 0) { (void) retsz; return 0; }

   ssize_t Write(const void *buffer, off_t offset, size_t size)
   {
      if (offset < 0) return -EINVAL;
      if (m_bytes.size() < (size_t) offset + size)
         m_bytes.resize((size_t) offset + size);
      memcpy(m_bytes.data() + offset, buffer, size);
      return (ssize_t) size;
   }
};

const long long BSIZE = 128 * 1024;

class InfoTest : public ::testing::Test
{
protected:
   XrdSysTrace trace { "PfcInfoTest" };
   size_t      m_saved_max_access = 0;

   void SetUp() override    { m_saved_max_access = Info::s_maxNumAccess; }
   void TearDown() override { Info::s_maxNumAccess = m_saved_max_access; }

   // An Info for a file of n_blocks whole blocks plus tail_bytes.
   //
   // By pointer, never by value: Info owns three malloc'd bit vectors and an
   // XrdCksCalc, but declares no copy or move constructor, so the implicit
   // shallow copy would double free. Returning one by value only appears to
   // work because NRVO elides the copy; it double frees under
   // -fno-elide-constructors, and did so on a CI compiler.
   std::unique_ptr<Info> make(long long n_blocks, long long tail_bytes = 0)
   {
      auto info = std::make_unique<Info>(&trace);
      info->SetBufferSizeFileSizeAndCreationTime(BSIZE, n_blocks * BSIZE + tail_bytes);
      return info;
   }

   // Mark a block present the way File does: written drives the in-memory
   // completeness bookkeeping, synced is what Write() actually persists.
   static void mark(Info &info, int block)
   {
      info.SetBitWritten(block);
      info.SetBitSynced(block);
   }
};

//==============================================================================
// Block accounting
//==============================================================================

TEST_F(InfoTest, BlockCountRoundsUpForPartialLastBlock)
{
   // A file that does not end on a block boundary still needs a bit for the
   // tail, otherwise the last bytes could never be recorded as present.
   EXPECT_EQ(make(4)->GetNBlocks(), 4);
   EXPECT_EQ(make(4, 1)->GetNBlocks(), 5);
   EXPECT_EQ(make(4, BSIZE - 1)->GetNBlocks(), 5);
   EXPECT_EQ(make(0, 1)->GetNBlocks(), 1);

   // One byte of bit vector per eight blocks, rounded up.
   EXPECT_EQ(make(8)->GetBitvecSizeInBytes(), 1);
   EXPECT_EQ(make(9)->GetBitvecSizeInBytes(), 2);
}

TEST_F(InfoTest, CompletenessTracksTheBitVector)
{
   auto info = make(9);

   EXPECT_FALSE(info->IsComplete());
   EXPECT_EQ(info->GetNDownloadedBlocks(), 0);

   for (int i = 0; i < 8; ++i)
   {
      mark(*info, i);
      EXPECT_FALSE(info->IsComplete()) << "still missing block 8 after setting " << i;
   }
   EXPECT_EQ(info->GetNDownloadedBlocks(), 8);

   // Crossing the byte boundary of the bit vector is where an off-by-one in
   // the addressing would show up.
   EXPECT_FALSE(info->TestBitWritten(8));
   mark(*info, 8);
   EXPECT_TRUE(info->TestBitWritten(8));
   EXPECT_TRUE(info->IsComplete());
   EXPECT_EQ(info->GetNDownloadedBlocks(), 9);
}

TEST_F(InfoTest, CountsBlocksMissingInARange)
{
   auto info = make(16);
   for (int i : {0, 1, 2, 8, 15}) mark(*info, i);

   EXPECT_EQ(info->CountBlocksNotWrittenInRng(0, 3), 0);
   EXPECT_EQ(info->CountBlocksNotWrittenInRng(0, 8), 5);   // 3,4,5,6,7
   EXPECT_EQ(info->CountBlocksNotWrittenInRng(3, 8), 5);
   EXPECT_EQ(info->CountBlocksNotWrittenInRng(15, 16), 0);
}

TEST_F(InfoTest, ExpectedDataFileSizeFollowsTheLastPresentBlock)
{
   // The data file is only as long as its last present block, except when that
   // block is the final one, where it is the real file size -- the tail block
   // is short. Purge and the disk-usage accounting rely on this.
   auto info = make(4, 100);            // 5 blocks, last one 100 bytes
   EXPECT_EQ(info->GetLastDownloadedBlock(), -1);

   mark(*info, 0);
   EXPECT_EQ(info->GetLastDownloadedBlock(), 0);
   EXPECT_EQ(info->GetExpectedDataFileSize(), BSIZE);

   mark(*info, 2);
   EXPECT_EQ(info->GetExpectedDataFileSize(), 3 * BSIZE);

   mark(*info, 4);
   EXPECT_EQ(info->GetExpectedDataFileSize(), 4 * BSIZE + 100);
}

//==============================================================================
// The on-disk round trip
//==============================================================================

TEST_F(InfoTest, RoundTripPreservesStateAndAccessHistory)
{
   MemFile f;
   Stats   s;
   s.m_NumIos        = 3;
   s.m_Duration      = 42;
   s.m_BytesHit      = 1000;
   s.m_BytesMissed   = 2000;
   s.m_BytesBypassed = 30;

   const std::vector<int> present { 0, 1, 5, 12 };

   {
      auto info = make(16);
      for (int i : present) mark(*info, i);
      info->SetCkSumState(CSChk_Cache);

      info->WriteIOStatAttach();
      info->WriteIOStatDetach(s);

      ASSERT_TRUE(info->Write(&f, "/mem", "test.cinfo"));
   }

   Info back(&trace);
   ASSERT_TRUE(back.Read(&f, "/mem", "test.cinfo"));

   EXPECT_EQ(back.GetVersion(), 4);
   EXPECT_EQ(back.GetFileSize(), 16 * BSIZE);
   EXPECT_EQ(back.GetBufferSize(), BSIZE);
   EXPECT_EQ(back.GetNBlocks(), 16);
   EXPECT_EQ(back.GetCkSumState(), CSChk_Cache);

   // Write() persists the synced vector and Read() copies it into the written
   // one, so exactly the marked blocks must come back present.
   for (int i = 0; i < 16; ++i)
   {
      bool want = std::find(present.begin(), present.end(), i) != present.end();
      EXPECT_EQ(back.TestBitWritten(i), want) << "block " << i;
   }
   EXPECT_EQ(back.GetNDownloadedBlocks(), (int) present.size());
   EXPECT_FALSE(back.IsComplete());

   ASSERT_EQ(back.GetAccessCnt(), 1u);
   ASSERT_EQ(back.RefAStats().size(), 1u);

   const Info::AStat &a = back.RefAStats()[0];
   EXPECT_EQ(a.NumIos, 3);
   EXPECT_EQ(a.Duration, 42);
   EXPECT_EQ(a.BytesHit, 1000);
   EXPECT_EQ(a.BytesMissed, 2000);
   EXPECT_EQ(a.BytesBypassed, 30);
   EXPECT_NE(a.DetachTime, 0);
}

TEST_F(InfoTest, RoundTripOfACompleteFileStaysComplete)
{
   MemFile f;
   {
      auto info = make(8);
      for (int i = 0; i < 8; ++i) mark(*info, i);
      ASSERT_TRUE(info->IsComplete());
      ASSERT_TRUE(info->Write(&f, "/mem"));
   }

   Info back(&trace);
   ASSERT_TRUE(back.Read(&f, "/mem"));
   EXPECT_TRUE(back.IsComplete());
   EXPECT_EQ(back.GetNDownloadedBytes(), 8 * BSIZE);
}

//==============================================================================
// Damaged files. Every one of these has to be refused: a cinfo that reads back
// as "fine" when it is not means serving corrupt data as a cache hit.
//==============================================================================

TEST_F(InfoTest, RefusesAnEmptyOrTruncatedFile)
{
   MemFile good;
   {
      auto info = make(16);
      for (int i = 0; i < 16; ++i) mark(*info, i);
      info->WriteIOStatSingle(1234);
      ASSERT_TRUE(info->Write(&good, "/mem"));
   }
   ASSERT_GT(good.m_bytes.size(), 16u);

   {
      MemFile empty;
      Info back(&trace);
      EXPECT_FALSE(back.Read(&empty, "/mem")) << "an empty cinfo must not read as valid";
   }

   // Cut at a few points: inside the header, inside the bit vector, and just
   // short of the trailing checksum.
   for (size_t cut : { size_t(2), good.m_bytes.size() / 2, good.m_bytes.size() - 1 })
   {
      MemFile t;
      t.m_bytes.assign(good.m_bytes.begin(), good.m_bytes.begin() + cut);
      Info back(&trace);
      EXPECT_FALSE(back.Read(&t, "/mem")) << "truncated to " << cut << " bytes";
   }
}

TEST_F(InfoTest, RefusesAnySingleByteCorruption)
{
   // Two crc32c checksums cover the file: one over the header struct, one over
   // the bit vector and the access records. Between them, and the version
   // check in front, no single byte of a cinfo should be able to change
   // without the file being refused. Sweeping every position rather than a
   // couple of hand-picked ones is what makes this a statement about the
   // format instead of about two offsets.
   MemFile good;
   {
      auto info = make(64);
      for (int i = 0; i < 64; i += 3) mark(*info, i);
      info->WriteIOStatSingle(1234, 1000, 1100);
      info->WriteIOStatSingle(5678, 2000, 2100);
      info->SetCkSumState(CSChk_Both);
      ASSERT_TRUE(info->Write(&good, "/mem"));
   }

   // The good file really does read back, so a false below means the damage
   // was caught and not that Read fails on this input anyway.
   {
      Info back(&trace);
      ASSERT_TRUE(back.Read(&good, "/mem"));
   }

   for (size_t pos = 0; pos < good.m_bytes.size(); ++pos)
   {
      MemFile t = good;
      t.m_bytes[pos] = (char) (t.m_bytes[pos] ^ 0xFF);

      Info back(&trace);
      EXPECT_FALSE(back.Read(&t, "/mem"))
         << "corruption at byte " << pos << " of " << good.m_bytes.size()
         << " was not detected";
   }
}

TEST_F(InfoTest, RefusesAnUnknownVersion)
{
   MemFile f;
   {
      auto info = make(4);
      mark(*info, 0);
      ASSERT_TRUE(info->Write(&f, "/mem"));
   }

   // Versions 2 and 3 have their own read paths; anything else has to be
   // rejected rather than parsed as the current layout.
   const int bogus = 99;
   memcpy(f.m_bytes.data(), &bogus, sizeof(bogus));

   Info back(&trace);
   EXPECT_FALSE(back.Read(&f, "/mem"));
}

//==============================================================================
// Access records
//==============================================================================

TEST_F(InfoTest, AStatMergeAccumulatesAndKeepsTheLaterDetachTime)
{
   Info::AStat a;
   a.AttachTime = 100; a.DetachTime = 110; a.NumIos = 1; a.Duration = 10;
   a.BytesHit = 1; a.BytesMissed = 2; a.BytesBypassed = 3;

   Info::AStat b;
   b.AttachTime = 200; b.DetachTime = 260; b.NumIos = 2; b.Duration = 60;
   b.BytesHit = 10; b.BytesMissed = 20; b.BytesBypassed = 30;

   a.MergeWith(b);

   EXPECT_EQ(a.AttachTime, 100);     // the earlier access still opens the record
   EXPECT_EQ(a.DetachTime, 260);     // and the later one closes it
   EXPECT_EQ(a.NumIos, 3);
   EXPECT_EQ(a.Duration, 70);
   EXPECT_EQ(a.NumMerged, 1);
   EXPECT_EQ(a.BytesHit, 11);
   EXPECT_EQ(a.BytesMissed, 22);
   EXPECT_EQ(a.BytesBypassed, 33);
}

TEST_F(InfoTest, AccessHistoryIsCappedButTotalsAreNotLost)
{
   Info::s_maxNumAccess = 4;

   const int n_acc = 20;
   long long total_hit = 0;

   MemFile f;
   {
      auto info = make(4);
      mark(*info, 0);
      for (int i = 0; i < n_acc; ++i)
      {
         // Distinct times, so compactification has something to choose by.
         info->WriteIOStatSingle(100 + i, 1000 + 10 * i, 1005 + 10 * i);
         total_hit += 100 + i;
      }
      ASSERT_TRUE(info->Write(&f, "/mem"));
   }

   Info back(&trace);
   ASSERT_TRUE(back.Read(&f, "/mem"));

   // The history is bounded so the cinfo cannot grow without limit ...
   EXPECT_LE(back.RefAStats().size(), Info::s_maxNumAccess);
   EXPECT_GT(back.RefAStats().size(), 0u);

   // ... but the count of accesses and the bytes they moved must survive the
   // merging, otherwise usage reporting silently under-reports busy files.
   EXPECT_EQ(back.GetAccessCnt(), (size_t) n_acc);

   long long kept_hit = 0;
   int       kept_ios = 0;
   for (const Info::AStat &a : back.RefAStats())
   {
      kept_hit += a.BytesHit;
      kept_ios += a.NumIos;
   }
   EXPECT_EQ(kept_hit, total_hit);
   EXPECT_EQ(kept_ios, n_acc);

   // Records are kept in time order, and the newest access is still the last.
   const std::vector<Info::AStat> &v = back.RefAStats();
   for (size_t i = 1; i < v.size(); ++i)
      EXPECT_LE(v[i - 1].AttachTime, v[i].AttachTime) << "record " << i;
   EXPECT_EQ(v.back().DetachTime, 1005 + 10 * (n_acc - 1));
}

TEST_F(InfoTest, ResetAllAccessStatsClearsTheHistory)
{
   auto info = make(4);
   info->WriteIOStatSingle(10);
   info->WriteIOStatSingle(20);
   ASSERT_EQ(info->GetAccessCnt(), 2u);

   info->ResetAllAccessStats();

   EXPECT_EQ(info->GetAccessCnt(), 0u);
   EXPECT_TRUE(info->RefAStats().empty());
}

//==============================================================================
// Checksum state
//==============================================================================

TEST_F(InfoTest, CkSumStateIsAThreeBitFieldAndDowngradeIntersects)
{
   auto info = make(4);

   EXPECT_EQ(info->GetCkSumState(), CSChk_None);

   info->SetCkSumState(CSChk_Both);
   EXPECT_TRUE(info->IsCkSumBoth());
   EXPECT_TRUE(info->IsCkSumCache());
   EXPECT_TRUE(info->IsCkSumNet());

   // Downgrading is an intersection with what the server is now configured
   // for: a file checksummed both ways is still valid for a cache-only setup,
   // but must lose the claim it cannot back any more.
   info->DowngradeCkSumState(CSChk_Cache);
   EXPECT_EQ(info->GetCkSumState(), CSChk_Cache);
   EXPECT_FALSE(info->IsCkSumNet());

   info->SetCkSumState(CSChk_Both);
   info->ResetCkSumNet();
   EXPECT_FALSE(info->IsCkSumNet());
   EXPECT_TRUE(info->IsCkSumCache());

   info->SetCkSumState(CSChk_Both);
   info->ResetCkSumCache();
   EXPECT_FALSE(info->IsCkSumCache());
   EXPECT_TRUE(info->IsCkSumNet());
}

TEST_F(InfoTest, CkSumStateSurvivesTheRoundTrip)
{
   for (int cs = CSChk_None; cs <= CSChk_Both; ++cs)
   {
      MemFile f;
      {
         auto info = make(4);
         mark(*info, 0);
         info->SetCkSumState((CkSumCheck_e) cs);
         ASSERT_TRUE(info->Write(&f, "/mem"));
      }
      Info back(&trace);
      ASSERT_TRUE(back.Read(&f, "/mem"));
      EXPECT_EQ(back.GetCkSumState(), (CkSumCheck_e) cs) << "state " << cs;
   }
}

}

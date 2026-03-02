#undef NDEBUG

#include <gtest/gtest.h>
#include <XrdSys/XrdSysStatx.hh>
#include <cstring>

using namespace testing;

class XrdSysStatxTests : public Test {};

TEST_F(XrdSysStatxTests, Stat2StatxBasicFields) {
  struct stat st;
  memset(&st, 0, sizeof(st));

  st.st_mode    = S_IFREG | 0755;
  st.st_nlink   = 3;
  st.st_uid     = 1000;
  st.st_gid     = 1001;
  st.st_ino     = 123456;
  st.st_size    = 4096;
  st.st_blocks  = 8;
  st.st_blksize = 512;

  XrdSysStatx stx;
  memset(&stx, 0xff, sizeof(stx));
  XrdSysStatxHelpers::Stat2Statx(st, stx);

  EXPECT_EQ(stx.stx_mask, (uint32_t)STATX_BASIC_STATS);

#ifdef __linux__
  EXPECT_EQ(stx.stx_mode,    (uint16_t)(S_IFREG | 0755));
  EXPECT_EQ(stx.stx_nlink,   (uint32_t)3);
  EXPECT_EQ(stx.stx_uid,     (uint32_t)1000);
  EXPECT_EQ(stx.stx_gid,     (uint32_t)1001);
  EXPECT_EQ(stx.stx_ino,     (uint64_t)123456);
  EXPECT_EQ(stx.stx_size,    (uint64_t)4096);
  EXPECT_EQ(stx.stx_blocks,  (uint64_t)8);
  EXPECT_EQ(stx.stx_blksize, (uint32_t)512);
#else
  EXPECT_EQ(stx.statx.st_mode,    st.st_mode);
  EXPECT_EQ(stx.statx.st_nlink,   st.st_nlink);
  EXPECT_EQ(stx.statx.st_uid,     st.st_uid);
  EXPECT_EQ(stx.statx.st_gid,     st.st_gid);
  EXPECT_EQ(stx.statx.st_ino,     st.st_ino);
  EXPECT_EQ(stx.statx.st_size,    st.st_size);
  EXPECT_EQ(stx.statx.st_blocks,  st.st_blocks);
  EXPECT_EQ(stx.statx.st_blksize, st.st_blksize);
#endif
}

TEST_F(XrdSysStatxTests, Statx2StatBasicFields) {
  struct stat st_orig;
  memset(&st_orig, 0, sizeof(st_orig));

  st_orig.st_mode    = S_IFREG | 0755;
  st_orig.st_nlink   = 3;
  st_orig.st_uid     = 1000;
  st_orig.st_gid     = 1001;
  st_orig.st_ino     = 123456;
  st_orig.st_size    = 4096;
  st_orig.st_blocks  = 8;
  st_orig.st_blksize = 512;

  XrdSysStatx stx;
  memset(&stx, 0, sizeof(stx));
  XrdSysStatxHelpers::Stat2Statx(st_orig, stx);

  struct stat st;
  memset(&st, 0xff, sizeof(st));
  XrdSysStatxHelpers::Statx2Stat(stx, st);

  EXPECT_EQ(st.st_mode,    st_orig.st_mode);
  EXPECT_EQ(st.st_nlink,   st_orig.st_nlink);
  EXPECT_EQ(st.st_uid,     st_orig.st_uid);
  EXPECT_EQ(st.st_gid,     st_orig.st_gid);
  EXPECT_EQ(st.st_ino,     st_orig.st_ino);
  EXPECT_EQ(st.st_size,    st_orig.st_size);
  EXPECT_EQ(st.st_blocks,  st_orig.st_blocks);
  EXPECT_EQ(st.st_blksize, st_orig.st_blksize);
}

#ifdef __linux__
TEST_F(XrdSysStatxTests, Statx2StatTimestamps) {
  XrdSysStatx stx;
  memset(&stx, 0, sizeof(stx));

  stx.stx_atime.tv_sec  = 1000000;
  stx.stx_atime.tv_nsec = 111;
  stx.stx_mtime.tv_sec  = 2000000;
  stx.stx_mtime.tv_nsec = 222;
  stx.stx_ctime.tv_sec  = 3000000;
  stx.stx_ctime.tv_nsec = 333;

  stx.stx_mask |= STATX_ATIME;
  stx.stx_mask |= STATX_MTIME;
  stx.stx_mask |= STATX_CTIME;

  struct stat st;
  XrdSysStatxHelpers::Statx2Stat(stx, st);

  EXPECT_EQ(st.st_atim.tv_sec,  1000000);
  EXPECT_EQ(st.st_atim.tv_nsec, 111);
  EXPECT_EQ(st.st_mtim.tv_sec,  2000000);
  EXPECT_EQ(st.st_mtim.tv_nsec, 222);
  EXPECT_EQ(st.st_ctim.tv_sec,  3000000);
  EXPECT_EQ(st.st_ctim.tv_nsec, 333);
}

TEST_F(XrdSysStatxTests, Statx2StatDeviceNumbers) {
  XrdSysStatx stx;
  memset(&stx, 0, sizeof(stx));

  stx.stx_dev_major  = 8;
  stx.stx_dev_minor  = 1;
  stx.stx_rdev_major = 253;
  stx.stx_rdev_minor = 0;
  stx.stx_mask |= STATX_BASIC_STATS;

  struct stat st;
  XrdSysStatxHelpers::Statx2Stat(stx, st);

  EXPECT_EQ(major(st.st_dev),  (unsigned)8);
  EXPECT_EQ(minor(st.st_dev),  (unsigned)1);
  EXPECT_EQ(major(st.st_rdev), (unsigned)253);
  EXPECT_EQ(minor(st.st_rdev), (unsigned)0);
}
#endif

#ifdef __linux__
TEST_F(XrdSysStatxTests, Stat2StatxTimestamps) {
  struct stat st;
  memset(&st, 0, sizeof(st));

  st.st_atim.tv_sec  = 1000000;
  st.st_atim.tv_nsec = 111;
  st.st_mtim.tv_sec  = 2000000;
  st.st_mtim.tv_nsec = 222;
  st.st_ctim.tv_sec  = 3000000;
  st.st_ctim.tv_nsec = 333;

  XrdSysStatx stx;
  XrdSysStatxHelpers::Stat2Statx(st, stx);

  EXPECT_EQ(stx.stx_atime.tv_sec,  1000000);
  EXPECT_EQ(stx.stx_atime.tv_nsec, 111);
  EXPECT_EQ(stx.stx_mtime.tv_sec,  2000000);
  EXPECT_EQ(stx.stx_mtime.tv_nsec, 222);
  EXPECT_EQ(stx.stx_ctime.tv_sec,  3000000);
  EXPECT_EQ(stx.stx_ctime.tv_nsec, 333);
}

TEST_F(XrdSysStatxTests, Stat2StatxDeviceNumbers) {
  struct stat st;
  memset(&st, 0, sizeof(st));

  st.st_dev  = makedev(8, 1);
  st.st_rdev = makedev(253, 0);

  XrdSysStatx stx;
  XrdSysStatxHelpers::Stat2Statx(st, stx);

  EXPECT_EQ(stx.stx_dev_major,  (uint32_t)8);
  EXPECT_EQ(stx.stx_dev_minor,  (uint32_t)1);
  EXPECT_EQ(stx.stx_rdev_major, (uint32_t)253);
  EXPECT_EQ(stx.stx_rdev_minor, (uint32_t)0);
}

TEST_F(XrdSysStatxTests, Stat2StatxZeroInitializesUnmappedFields) {
  struct stat st;
  memset(&st, 0, sizeof(st));
  st.st_size = 100;

  XrdSysStatx stx;
  memset(&stx, 0xff, sizeof(stx));
  XrdSysStatxHelpers::Stat2Statx(st, stx);

  EXPECT_EQ(stx.stx_attributes, (uint64_t)0);
  EXPECT_EQ(stx.stx_btime.tv_sec, 0);
  EXPECT_EQ(stx.stx_btime.tv_nsec, 0);
}
#endif

TEST_F(XrdSysStatxTests, StatxT2StatT) {
  statx_timestamp stx_T;
  stx_T.tv_sec  = 1700000000;
  stx_T.tv_nsec = 123456789;

  struct timespec sta_T;
  memset(&sta_T, 0xff, sizeof(sta_T));
  XrdSysStatxHelpers::StatxT2StatT(stx_T, sta_T);

  EXPECT_EQ(sta_T.tv_sec,  1700000000);
  EXPECT_EQ(sta_T.tv_nsec, 123456789);
}

TEST_F(XrdSysStatxTests, StatT2StatxT) {
  struct timespec sta_T;
  sta_T.tv_sec  = 1700000000;
  sta_T.tv_nsec = 987654321;

  statx_timestamp stx_T;
  memset(&stx_T, 0xff, sizeof(stx_T));
  XrdSysStatxHelpers::StatT2StatxT(sta_T, stx_T);

  EXPECT_EQ(stx_T.tv_sec,  1700000000);
  EXPECT_EQ(stx_T.tv_nsec, 987654321);
}

//------------------------------------------------------------------------------
// Unit tests for XrdXrootdFileStats: the first error of a file, which the
// f-stream close record reports. These tests do not start the monitor.
//------------------------------------------------------------------------------

#include "XProtocol/XProtocol.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

// A file without an f-stream open record gets no close record, so its errors
// are not kept
TEST(FileStats, SetErrIgnoresAFileWithoutAMonitorEntry)
{
  XrdXrootdFileStats stats;
  stats.Init();
  stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
  EXPECT_EQ(stats.firstErr().cat, 0);
}

// The error of a file with a monitor entry is kept
TEST(FileStats, SetErrKeepsTheCodeAndTheCategory)
{
  XrdXrootdFileStats stats;
  stats.Init();
  stats.MonEnt = 0;
  stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
  EXPECT_EQ(stats.firstErr().code, kXR_IOError);
  EXPECT_EQ(stats.firstErr().cat, XrdXrootdMonStatERR::errRead);
}

// A later error does not replace the first error
TEST(FileStats, SetErrKeepsOnlyTheFirstError)
{
  XrdXrootdFileStats stats;
  stats.Init();
  stats.MonEnt = 0;
  stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
  stats.setErr(kXR_FSError, XrdXrootdMonStatERR::errClose);
  EXPECT_EQ(stats.firstErr().code, kXR_IOError);
  EXPECT_EQ(stats.firstErr().cat, XrdXrootdMonStatERR::errRead);
}

// Two streams of one file can fail at the same time. The kept error must then
// be one of the two errors, never the code of one error with the category of
// the other. A race does not occur at each try, so the test makes many tries.
// The time limit keeps the test short when threads run slowly (valgrind).
TEST(FileStats, ConcurrentSetErrKeepsTheCodeWithItsCategory)
{
  const int maxTries = 5000000;
  const int stop = -1;
  const auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  XrdXrootdFileStats stats;
  std::atomic<int> round{0};
  std::atomic<int> done{0};

  auto fail = [&](int code, char cat) {
    for (int r = 1;; r++) {
      int now;
      while ((now = round.load()) != r && now != stop) {}
      if (now == stop)
        return;
      stats.setErr(code, cat);
      done++;
    }
  };

  std::thread reader(fail, kXR_IOError, XrdXrootdMonStatERR::errRead);
  std::thread writer(fail, kXR_FSError, XrdXrootdMonStatERR::errWrite);

  int nMixed = 0;
  for (int r = 1; r <= maxTries && std::chrono::steady_clock::now() < endTime;
       r++) {
    stats.Init();
    stats.MonEnt = 0;
    round = r;
    while (done.load() < 2 * r) {}
    const XrdXrootdFileStats::ErrRec err = stats.firstErr();
    const bool isRead = err.code == kXR_IOError
                     && err.cat == XrdXrootdMonStatERR::errRead;
    const bool isWrite = err.code == kXR_FSError
                      && err.cat == XrdXrootdMonStatERR::errWrite;
    if (!isRead && !isWrite)
      nMixed++;
  }
  round = stop;
  reader.join();
  writer.join();

  EXPECT_EQ(nMixed, 0) << "errors with the code of one error and the "
                          "category of the other";
}

// A thread that reads the first error while another thread sets it must see
// the code together with the category. A race does not occur at each try, so
// the test makes many tries.
TEST(FileStats, ReaderSeesTheCodeWithTheCategory)
{
  const int maxTries = 5000000;
  const int stop = -1;
  const auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  XrdXrootdFileStats stats;
  std::atomic<int> round{0};
  std::atomic<int> done{0};
  std::atomic<int> nBad{0};

  std::thread writer([&] {
    for (int r = 1;; r++) {
      int now;
      while ((now = round.load()) != r && now != stop) {}
      if (now == stop)
        return;
      stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
      done++;
    }
  });

  std::thread reader([&] {
    for (int r = 1;; r++) {
      int now;
      while ((now = round.load()) != r && now != stop) {}
      if (now == stop)
        return;
      XrdXrootdFileStats::ErrRec err;
      while ((err = stats.firstErr()).cat == 0) {}
      if (err.code != kXR_IOError)
        nBad++;
      done++;
    }
  });

  for (int r = 1; r <= maxTries && std::chrono::steady_clock::now() < endTime;
       r++) {
    stats.Init();
    stats.MonEnt = 0;
    round = r;
    while (done.load() < 2 * r) {}
  }
  round = stop;
  writer.join();
  reader.join();

  EXPECT_EQ(nBad.load(), 0) << "reads that saw the category without its code";
}

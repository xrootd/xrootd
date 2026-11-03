#undef NDEBUG

#include "XrdHttp/XrdHttpReadRangeHandler.hh"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

using namespace testing;

TEST(ReadRangeHandlerTest, ReadsTwoMaxChunkSizeRangesAsTwoChunks) {
    long long filesize = 8;
    int rangeBegin = 0;
    int rangeEnd = 3;
    int rangeBegin2 = 4;
    int rangeEnd2 = 7;
    int readvMaxChunkSize = 4;
    int readvMaxChunks = 20;
    int rReqMaxSize = 200;
    std::stringstream ss;
    ss << "bytes=" << rangeBegin << "-" << rangeEnd << ", " << rangeBegin2 << "-" << rangeEnd2;
    std::string rs = ss.str();
    XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
    XrdHttpReadRangeHandler h(cfg);
    h.ParseContentRange(rs.c_str());
    h.SetFilesize(filesize);
    const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(2u, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(4, cl[0].size);
    ASSERT_EQ(4, cl[1].offset);
    ASSERT_EQ(4, cl[1].size);
    ASSERT_EQ(2u, ul.size());
}

TEST(ReadRangeHandlerTest, ReadsASmallRangeAsOneChunk) {
  long long filesize = 8;
  int rangeBegin = 0;
  int rangeEnd = 3;
  int readvMaxChunkSize = 5;
  int readvMaxChunks = 20;
  int rReqMaxSize = 200;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << rangeEnd;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  const XrdHttpIOList &cl = h.NextReadList();
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(4, cl[0].size);
  ASSERT_EQ(1u, ul.size());
}

TEST(ReadRangeHandlerTest, SplitsLargeRangeOnlyInAMultiRangeRequest) {
  long long filesize = 8;
  int rangeBegin = 0;
  int rangeEnd = 7;
  int readvMaxChunkSize = 3;
  int readvMaxChunks = 20;
  int rReqMaxSize = 200;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << rangeEnd;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  {
    const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(8, cl[0].size);
    ASSERT_EQ(1u, ul.size());
  }
  ss.str("");
  ss << "bytes=0-0," << rangeBegin << "-" << rangeEnd;
  rs = ss.str();
  h.reset();
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  {
    const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(4u, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(1, cl[0].size);
    ASSERT_EQ(0, cl[1].offset);
    ASSERT_EQ(3, cl[1].size);
    ASSERT_EQ(3, cl[2].offset);
    ASSERT_EQ(3, cl[2].size);
    ASSERT_EQ(6, cl[3].offset);
    ASSERT_EQ(2, cl[3].size);
    ASSERT_EQ(2u, ul.size());
  }
}

TEST(ReadRangeHandlerTest, ReadsAnOpenEndedRangeInMaxRequestSizeSteps) {
  long long filesize = 200;
  int rangeBegin = 0;
  int readvMaxChunkSize = 4;
  int readvMaxChunks = 20;
  int rReqMaxSize = 100;
  bool start, finish;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << "\r";
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  {
    const XrdHttpIOList &cl1 = h.NextReadList();
    ASSERT_EQ(1u, ul.size());
    ASSERT_EQ(1u, cl1.size());
    ASSERT_EQ(0, cl1[0].offset);
    ASSERT_EQ(100, cl1[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(100, nullptr, start, finish));
  }
  {
    const XrdHttpIOList &cl2 = h.NextReadList();
    ASSERT_EQ(1u, cl2.size());
    ASSERT_EQ(100, cl2[0].offset);
    ASSERT_EQ(100, cl2[0].size);
  }
}

TEST(ReadRangeHandlerTest, ReadsTheLastBytesForASuffixRange) {
  long long filesize = 200;
  int rangeEnd = 5;
  int readvMaxChunkSize = 4;
  int readvMaxChunks = 20;
  int rReqMaxSize = 100;
  bool start, finish;
  std::stringstream ss;
  ss << "bytes=-" << rangeEnd << "\r";
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  {
    const XrdHttpIOList &cl1 = h.NextReadList();
    ASSERT_EQ(1u, ul.size());
    ASSERT_EQ(1u, cl1.size());
    ASSERT_EQ(195, cl1[0].offset);
    ASSERT_EQ(5, cl1[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
  }
  {
    const XrdHttpIOList &cl2 = h.NextReadList();
    ASSERT_EQ(0u, cl2.size());
    const XrdHttpReadRangeHandler::Error &error = h.getError();
    ASSERT_EQ(false, static_cast<bool>(error));
  }
}

TEST(ReadRangeHandlerTest, ReadsOneByteForRangeZeroToZero) {
  long long filesize = 8;
  int rangeBegin = 0;
  int rangeEnd = 0;
  int readvMaxChunkSize = 4;
  int readvMaxChunks = 20;
  int rReqMaxSize = 100;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << rangeEnd;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  const XrdHttpIOList &cl = h.NextReadList();
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(1u, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(0, ul[0].end);
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(1, cl[0].size);
}

TEST(ReadRangeHandlerTest, TruncatesTheRangeEndAtTheFileSize) {
  long long filesize = 2;
  int rangeBegin = 0;
  int rangeEnd = 4;
  int readvMaxChunkSize = 10;
  int readvMaxChunks = 20;
  int rReqMaxSize = 100;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << rangeEnd;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  const XrdHttpIOList &cl = h.NextReadList();
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(2, cl[0].size);
  ASSERT_EQ(1u, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(1, ul[0].end);
}

TEST(ReadRangeHandlerTest, Returns416ForARangeAfterTheFileEnd) {
  long long filesize = 2;
  int rangeBegin = 4;
  int rangeEnd = 6;
  int readvMaxChunkSize = 10;
  int readvMaxChunks = 20;
  int rReqMaxSize = 100;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << rangeEnd;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  const XrdHttpIOList &cl = h.NextReadList();
  ASSERT_EQ(0u, ul.size());
  ASSERT_EQ(0u, cl.size());
  const XrdHttpReadRangeHandler::Error &error = h.getError();
  ASSERT_EQ(true, static_cast<bool>(error));
  ASSERT_EQ(416, error.httpRetCode);
}

TEST(ReadRangeHandlerTest, DropsARangeOutsideTheFile) {
  long long filesize = 20;
  int rangeBegin1 = 22;
  int rangeEnd1 = 30;
  int rangeBegin2 = 4;
  int rangeEnd2 = 6;
  int readvMaxChunkSize = 10;
  int readvMaxChunks = 20;
  int rReqMaxSize = 100;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin1 << "-" << rangeEnd1 << "," << rangeBegin2 << "-" << rangeEnd2;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  const XrdHttpIOList &cl = h.NextReadList();
  ASSERT_EQ(1u, ul.size());
  ASSERT_EQ(4, ul[0].start);
  ASSERT_EQ(6, ul[0].end);
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(4, cl[0].offset);
  ASSERT_EQ(3, cl[0].size);
}

TEST(ReadRangeHandlerTest, ReadsOneRangeInStepsAndFlagsStartAndEnd) {
  long long filesize = 20;
  int rangeBegin = 0;
  int rangeEnd = 15;
  int readvMaxChunkSize = 3;
  int readvMaxChunks = 20;
  int rReqMaxSize = 5;
  bool start, finish;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin << "-" << rangeEnd;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  ASSERT_EQ(1u, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(15, ul[0].end);
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
    ASSERT_EQ(true, start);
    ASSERT_EQ(false, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(5, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(10, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(15, cl[0].offset);
    ASSERT_EQ(1, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(1, nullptr, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(true, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(0u, cl.size());
    const XrdHttpReadRangeHandler::Error &error = h.getError();
    ASSERT_EQ(false, static_cast<bool>(error));
  }
}

TEST(ReadRangeHandlerTest, MapsEachChunkOfTwoRangesToItsUserRange) {
  long long filesize = 22;
  int rangeBegin1 = 0;
  int rangeEnd1 = 1;
  int rangeBegin2 = 5;
  int rangeEnd2 = 21;
  int readvMaxChunkSize = 3;
  int readvMaxChunks = 2;
  int rReqMaxSize = 5;
  bool start, finish;
  const XrdHttpReadRangeHandler::UserRange  *ur;
  std::stringstream ss;
  ss << "bytes=" << rangeBegin1 << "-" << rangeEnd1 << "," << rangeBegin2 << "-" << rangeEnd2;
  std::string rs = ss.str();
  XrdHttpReadRangeHandler::Configuration cfg(readvMaxChunkSize, readvMaxChunks, rReqMaxSize);
  XrdHttpReadRangeHandler h(cfg);
  h.ParseContentRange(rs.c_str());
  h.SetFilesize(filesize);
  const XrdHttpReadRangeHandler::UserRangeList &ul = h.ListResolvedRanges();
  ASSERT_EQ(2u, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(1, ul[0].end);
  ASSERT_EQ(5, ul[1].start);
  ASSERT_EQ(21, ul[1].end);
  {
    // we get 0-1, 5-7
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(2u, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(2, cl[0].size);
    ASSERT_EQ(5, cl[1].offset);
    ASSERT_EQ(3, cl[1].size);
    ASSERT_EQ(0, h.NotifyReadResult(2, &ur, start, finish));
    ASSERT_EQ(true, start);
    ASSERT_EQ(false, finish);
    ASSERT_EQ(0, ur->start);
    ASSERT_EQ(1, ur->end);
    ASSERT_EQ(0, h.NotifyReadResult(3, &ur, start, finish));
    ASSERT_EQ(true, start);
    ASSERT_EQ(false, finish);
    ASSERT_EQ(5, ur->start);
    ASSERT_EQ(21, ur->end);
  }
  {
    // we get 8-12
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(8, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, &ur, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
    ASSERT_EQ(5, ur->start);
    ASSERT_EQ(21, ur->end);
  }
  {
    // we get 13-17
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1u, cl.size());
    ASSERT_EQ(13, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, &ur, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
    ASSERT_EQ(5, ur->start);
    ASSERT_EQ(21, ur->end);
  }
  {
    // we get 18-20, 21-21
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(2u, cl.size());
    ASSERT_EQ(18, cl[0].offset);
    ASSERT_EQ(3, cl[0].size);
    ASSERT_EQ(21, cl[1].offset);
    ASSERT_EQ(1, cl[1].size);
    ASSERT_EQ(0, h.NotifyReadResult(3, &ur, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
    ASSERT_EQ(5, ur->start);
    ASSERT_EQ(21, ur->end);
    ASSERT_EQ(0, h.NotifyReadResult(1, &ur, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(true, finish);
    ASSERT_EQ(5, ur->start);
    ASSERT_EQ(21, ur->end);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(0u, cl.size());
    const XrdHttpReadRangeHandler::Error &error = h.getError();
    ASSERT_EQ(false, static_cast<bool>(error));
  }
}

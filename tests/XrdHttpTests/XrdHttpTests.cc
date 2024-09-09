#undef NDEBUG

#include "XrdHttp/XrdHttpReq.hh"
#include "XrdHttp/XrdHttpProtocol.hh"
#include "XrdHttp/XrdHttpChecksumHandler.hh"
#include "XrdHttp/XrdHttpReadRangeHandler.hh"
#include <exception>
#include <gtest/gtest.h>
#include <string>
#include <sstream>


using namespace testing;

class XrdHttpTests : public Test {};

TEST(XrdHttpTests, checksumHandlerTests) {
    {
        XrdHttpChecksumHandlerImpl handler;
        handler.configure("0:sha512,1:crc32");
        auto configuredChecksum = handler.getConfiguredChecksums();
        ASSERT_EQ(2, configuredChecksum.size());
        ASSERT_EQ("sha512", configuredChecksum[0]->getXRootDConfigDigestName());
        ASSERT_EQ("crc32", configuredChecksum[1]->getXRootDConfigDigestName());
    }
    {
        XrdHttpChecksumHandlerImpl handler;
        handler.configure("0:sha512,1:crc32,2:does_not_exist");
        auto configuredChecksum = handler.getConfiguredChecksums();
        auto incompatibleChecksums = handler.getNonIANAConfiguredCksums();
        ASSERT_EQ(1,incompatibleChecksums.size());
        ASSERT_EQ("does_not_exist",incompatibleChecksums[0]);
        ASSERT_EQ(2,configuredChecksum.size());
    }
}

TEST(XrdHttpTests, checksumHandlerSelectionTest) {
    {
        //one algorithm, HTTP-IANA compatible
        std::string reqDigest = "adler32";
        const char *configChecksumList = "0:adler32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRun(reqDigest);
        ASSERT_EQ("adler32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        //sha-512, same as sha512, it is HTTP-IANA compatible
        std::string reqDigest = "sha-512";
        const char *configChecksumList = "0:sha-512";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRun(reqDigest);
        ASSERT_EQ("sha-512",cksumToRun->getXRootDConfigDigestName());
        ASSERT_EQ("sha-512",cksumToRun->getHttpName());
        ASSERT_EQ(true,cksumToRun->needsBase64Padding());
    }
    {
        //UNIXCksum
        std::string reqDigest = "UNIXcksum";
        {
            const char *configChecksumList = "0:cksum";
            XrdHttpChecksumHandlerImpl handler;
            handler.configure(configChecksumList);
            auto cksumToRun = handler.getChecksumToRun(reqDigest);
            ASSERT_EQ("cksum",cksumToRun->getXRootDConfigDigestName());
            ASSERT_EQ("UNIXcksum",cksumToRun->getHttpName());
        }
        {
            const char *configChecksumList = "0:unixcksum";
            XrdHttpChecksumHandlerImpl handler;
            handler.configure(configChecksumList);
            auto cksumToRun = handler.getChecksumToRun(reqDigest);
            ASSERT_EQ("unixcksum",cksumToRun->getXRootDConfigDigestName());
            ASSERT_EQ("UNIXcksum",cksumToRun->getHttpName());
            ASSERT_FALSE(cksumToRun->needsBase64Padding());
        }
    }
    {
        //Multiple HTTP-IANA compatible checksum configured, the
        //checksum returned should be the first appearing in the reqDigest
        std::string reqDigest = "crc32,adler32";
        const char *configChecksumList = "0:adler32,1:crc32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRun(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        // If the requested digest does not exist, the first configured HTTP-IANA
        // compatible checksum will be ran
        std::string reqDigest = "DOES_NOT_EXIST";
        const char *configChecksumList = "0:does_not_exist_algo,1:crc32,2:adler32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRun(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        // If the requested digest contains at least one HTTP-IANA compatible
        // digest, then the HTTP-IANA compatible digest will be returned
        std::string reqDigest = "DOES_NOT_EXIST , crc32";
        const char *configChecksumList = "0:adler32,1:crc32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRun(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        //Ensure weighted digest (;q=xx) are discarded but still allows to get the correct algorithm
        //depending on the order of submission
        std::string reqDigest = "crc32;q=0.1,adler32;q=0.5";
        const char *configChecksumList = "0:adler32,1:crc32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRun(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        //sha-* algorithms
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = "0:crc32,1:sha512,2:sha-256";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        {
            auto cksumToRun = handler.getChecksumToRun(reqDigest);
            ASSERT_EQ("sha512",cksumToRun->getXRootDConfigDigestName());
        }
        {
            reqDigest = "sha512";
            auto cksumToRun = handler.getChecksumToRun(reqDigest);
            ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
            ASSERT_FALSE(cksumToRun->needsBase64Padding());
        }
        {
            reqDigest = "sha-256";
            auto cksumToRun = handler.getChecksumToRun(reqDigest);
            ASSERT_EQ("sha-256",cksumToRun->getXRootDConfigDigestName());
            ASSERT_EQ("sha-256",cksumToRun->getHttpName());
            ASSERT_TRUE(cksumToRun->needsBase64Padding());
        }
    }
    {
        //one sha-512 HTTP configured algorithm
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = "0:my_custom_sha512,1:second_custom_sha512,2:sha-512";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        ASSERT_EQ("sha-512", handler.getChecksumToRun(reqDigest)->getXRootDConfigDigestName());
        ASSERT_EQ("sha-512", handler.getChecksumToRun("adler32")->getXRootDConfigDigestName());
    }
    {
        //algorithm configured but none is compatible with HTTP
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = "0:my_custom_sha512,1:second_custom_sha512";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        ASSERT_EQ(nullptr, handler.getChecksumToRun(reqDigest));
    }
    {
        // no algorithm configured, should always return a nullptr
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = nullptr;
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        ASSERT_EQ(nullptr, handler.getChecksumToRun(reqDigest));
    }
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerTwoRangesOfSizeEqualToMaxChunkSize) {
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
    ASSERT_EQ(2, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(4, cl[0].size);
    ASSERT_EQ(4, cl[1].offset);
    ASSERT_EQ(4, cl[1].size);
    ASSERT_EQ(2, ul.size());
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerOneRangeSizeLessThanMaxChunkSize) {
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
  ASSERT_EQ(1, cl.size());
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(4, cl[0].size);
  ASSERT_EQ(1, ul.size());
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerOneRangeSizeGreaterThanMaxChunkSize) {
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
    ASSERT_EQ(1, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(8, cl[0].size);
    ASSERT_EQ(1, ul.size());
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
    ASSERT_EQ(4, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(1, cl[0].size);
    ASSERT_EQ(0, cl[1].offset);
    ASSERT_EQ(3, cl[1].size);
    ASSERT_EQ(3, cl[2].offset);
    ASSERT_EQ(3, cl[2].size);
    ASSERT_EQ(6, cl[3].offset);
    ASSERT_EQ(2, cl[3].size);
    ASSERT_EQ(2, ul.size());
  }
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerRange0ToEnd) {
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
    ASSERT_EQ(1, ul.size());
    ASSERT_EQ(1, cl1.size());
    ASSERT_EQ(0, cl1[0].offset);
    ASSERT_EQ(100, cl1[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(100, nullptr, start, finish));
  }
  {
    const XrdHttpIOList &cl2 = h.NextReadList();
    ASSERT_EQ(1, cl2.size());
    ASSERT_EQ(100, cl2[0].offset);
    ASSERT_EQ(100, cl2[0].size);
  }
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerRange5FromEnd) {
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
    ASSERT_EQ(1, ul.size());
    ASSERT_EQ(1, cl1.size());
    ASSERT_EQ(195, cl1[0].offset);
    ASSERT_EQ(5, cl1[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
  }
  {
    const XrdHttpIOList &cl2 = h.NextReadList();
    ASSERT_EQ(0, cl2.size());
    const XrdHttpReadRangeHandler::Error &error = h.getError();
    ASSERT_EQ(false, static_cast<bool>(error));
  }
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerRange0To0) {
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
  ASSERT_EQ(1, cl.size());
  ASSERT_EQ(1, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(0, ul[0].end);
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(1, cl[0].size);
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerEndByteGreaterThanFileSize) {
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
  ASSERT_EQ(1, cl.size());
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(2, cl[0].size);
  ASSERT_EQ(1, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(1, ul[0].end);
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerRangeBeginGreaterThanFileSize) {
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
  ASSERT_EQ(0, ul.size());
  ASSERT_EQ(0, cl.size());
  const XrdHttpReadRangeHandler::Error &error = h.getError();
  ASSERT_EQ(true, static_cast<bool>(error));
  ASSERT_EQ(416, error.httpRetCode);
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerTwoRangesOneOutsideFileExtent) {
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
  ASSERT_EQ(1, ul.size());
  ASSERT_EQ(4, ul[0].start);
  ASSERT_EQ(6, ul[0].end);
  ASSERT_EQ(1, cl.size());
  ASSERT_EQ(4, cl[0].offset);
  ASSERT_EQ(3, cl[0].size);
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerMultiChunksSingleRange) {
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
  ASSERT_EQ(1, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(15, ul[0].end);
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
    ASSERT_EQ(true, start);
    ASSERT_EQ(false, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1, cl.size());
    ASSERT_EQ(5, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1, cl.size());
    ASSERT_EQ(10, cl[0].offset);
    ASSERT_EQ(5, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(5, nullptr, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(false, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(1, cl.size());
    ASSERT_EQ(15, cl[0].offset);
    ASSERT_EQ(1, cl[0].size);
    ASSERT_EQ(0, h.NotifyReadResult(1, nullptr, start, finish));
    ASSERT_EQ(false, start);
    ASSERT_EQ(true, finish);
  }
  {
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(0, cl.size());
    const XrdHttpReadRangeHandler::Error &error = h.getError();
    ASSERT_EQ(false, static_cast<bool>(error));
  }
}

TEST(XrdHttpTests, xrdHttpReadRangeHandlerMultiChunksTwoRanges) {
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
  ASSERT_EQ(2, ul.size());
  ASSERT_EQ(0, ul[0].start);
  ASSERT_EQ(1, ul[0].end);
  ASSERT_EQ(5, ul[1].start);
  ASSERT_EQ(21, ul[1].end);
  {
    // we get 0-1, 5-7
    const XrdHttpIOList &cl = h.NextReadList();
    ASSERT_EQ(2, cl.size());
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
    ASSERT_EQ(1, cl.size());
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
    ASSERT_EQ(1, cl.size());
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
    ASSERT_EQ(2, cl.size());
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
    ASSERT_EQ(0, cl.size());
    const XrdHttpReadRangeHandler::Error &error = h.getError();
    ASSERT_EQ(false, static_cast<bool>(error));
  }
}

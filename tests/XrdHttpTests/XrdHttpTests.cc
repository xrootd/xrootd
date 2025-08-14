#undef NDEBUG

#include "XrdHttp/XrdHttpReq.hh"
#include "XrdHttp/XrdHttpProtocol.hh"
#include "XrdHttp/XrdHttpChecksumHandler.hh"
#include "XrdHttp/XrdHttpReadRangeHandler.hh"
#include "XrdHttp/XrdHttpHeaderUtils.hh"
#include "XrdHttpCors/XrdHttpCorsHandler.hh"
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
        ASSERT_EQ(2u, configuredChecksum.size());
        ASSERT_EQ("sha512", configuredChecksum[0]->getXRootDConfigDigestName());
        ASSERT_EQ("crc32", configuredChecksum[1]->getXRootDConfigDigestName());
    }
    {
        XrdHttpChecksumHandlerImpl handler;
        handler.configure("0:sha512,1:crc32,2:does_not_exist");
        auto configuredChecksum = handler.getConfiguredChecksums();
        auto incompatibleChecksums = handler.getNonIANAConfiguredCksums();
        ASSERT_EQ(1u,incompatibleChecksums.size());
        ASSERT_EQ("does_not_exist",incompatibleChecksums[0]);
        ASSERT_EQ(2u,configuredChecksum.size());
    }
}

TEST(XrdHttpTests, checksumHandlerSelectionTest) {
    {
        //one algorithm, HTTP-IANA compatible
        std::string reqDigest = "adler32";
        const char *configChecksumList = "0:adler32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
        ASSERT_EQ("adler32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        //sha-512, same as sha512, it is HTTP-IANA compatible
        std::string reqDigest = "sha-512";
        const char *configChecksumList = "0:sha-512";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
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
            auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
            ASSERT_EQ("cksum",cksumToRun->getXRootDConfigDigestName());
            ASSERT_EQ("UNIXcksum",cksumToRun->getHttpName());
        }
        {
            const char *configChecksumList = "0:unixcksum";
            XrdHttpChecksumHandlerImpl handler;
            handler.configure(configChecksumList);
            auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
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
        auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        // If the requested digest does not exist, the first configured HTTP-IANA
        // compatible checksum will be ran
        std::string reqDigest = "DOES_NOT_EXIST";
        const char *configChecksumList = "0:does_not_exist_algo,1:crc32,2:adler32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        // If the requested digest contains at least one HTTP-IANA compatible
        // digest, then the HTTP-IANA compatible digest will be returned
        std::string reqDigest = "DOES_NOT_EXIST , crc32";
        const char *configChecksumList = "0:adler32,1:crc32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        //Ensure weighted digest (;q=xx) are discarded but still allows to get the correct algorithm
        //depending on the order of submission
        std::string reqDigest = "crc32;q=0.1,adler32;q=0.5";
        const char *configChecksumList = "0:adler32,1:crc32";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
        ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
    }
    {
        //sha-* algorithms
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = "0:crc32,1:sha512,2:sha-256";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        {
            auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
            ASSERT_EQ("sha512",cksumToRun->getXRootDConfigDigestName());
        }
        {
            reqDigest = "sha512";
            auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
            ASSERT_EQ("crc32",cksumToRun->getXRootDConfigDigestName());
            ASSERT_FALSE(cksumToRun->needsBase64Padding());
        }
        {
            reqDigest = "sha-256";
            auto cksumToRun = handler.getChecksumToRunWantDigest(reqDigest);
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
        ASSERT_EQ("sha-512", handler.getChecksumToRunWantDigest(reqDigest)->getXRootDConfigDigestName());
        ASSERT_EQ("sha-512", handler.getChecksumToRunWantDigest("adler32")->getXRootDConfigDigestName());
    }
    {
        //algorithm configured but none is compatible with HTTP
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = "0:my_custom_sha512,1:second_custom_sha512";
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        ASSERT_EQ(nullptr, handler.getChecksumToRunWantDigest(reqDigest));
    }
    {
        // no algorithm configured, should always return a nullptr
        std::string reqDigest = "SHA-512";
        const char *configChecksumList = nullptr;
        XrdHttpChecksumHandlerImpl handler;
        handler.configure(configChecksumList);
        ASSERT_EQ(nullptr, handler.getChecksumToRunWantDigest(reqDigest));
    }
}

static std::vector<std::pair<std::map<std::string,uint8_t>,std::string>> wantReprDigestCksumNames {
  {{{"adler32",1}},"adler32"},
  {{{"adler",1}},"adler"},
  {{{"adler",1},{"sha-256",3}}, "sha-256"},
  {{{"DOES_NOT_EXIST",1}}, "adler32"},
  {{{"DOES_NOT_EXIST",1},{"sha-256",1}}, "sha-256"},
  {{{"adler32",1},{"sha-256",11}}, "sha-256"},
};

TEST(XrdHttpTests, checksumHandlerSelectionWantReprDigestTest) {
  for(const auto & wantReprDigestCksumName: wantReprDigestCksumNames) {
    XrdHttpChecksumHandlerImpl handler;
    handler.configure("0:adler32,1:sha-256");
    ASSERT_EQ(wantReprDigestCksumName.second,handler.getChecksumToRunWantReprDigest(wantReprDigestCksumName.first)->getHttpNameLowerCase());
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
    ASSERT_EQ(2u, cl.size());
    ASSERT_EQ(0, cl[0].offset);
    ASSERT_EQ(4, cl[0].size);
    ASSERT_EQ(4, cl[1].offset);
    ASSERT_EQ(4, cl[1].size);
    ASSERT_EQ(2u, ul.size());
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
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(4, cl[0].size);
  ASSERT_EQ(1u, ul.size());
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
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(1u, ul.size());
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
  ASSERT_EQ(1u, cl.size());
  ASSERT_EQ(0, cl[0].offset);
  ASSERT_EQ(2, cl[0].size);
  ASSERT_EQ(1u, ul.size());
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
  ASSERT_EQ(0u, ul.size());
  ASSERT_EQ(0u, cl.size());
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
  ASSERT_EQ(1u, ul.size());
  ASSERT_EQ(4, ul[0].start);
  ASSERT_EQ(6, ul[0].end);
  ASSERT_EQ(1u, cl.size());
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

static inline const std::pair<std::string,std::string> encodedDecodedStrings [] {
  {"zteos64%3AMDAF5PGJ4Wa12g%3D","zteos64:MDAF5PGJ4Wa12g="},
  //"zteos64%3BAMDAF5PGJ4Wa12g%3B%3B",
  {"xrootd_tpc-helloworld","xrootd_tpc-helloworld"},
  {"",""},
  //"zteos64:%%test",
  {"Bearer%20token","Bearer token"},
  {"%20%5B%5D%3A%23%3D%0A%0D"," []:#=\n\r"}
};

TEST(XrdHttpTests, strEncodeDecodeTest) {
  for(auto [encoded,decoded]: encodedDecodedStrings) {
    ASSERT_EQ(decode_str(encoded), decoded);
    ASSERT_EQ(encoded, encode_str(decoded));
    ASSERT_EQ(encode_str(decode_str(encoded)), encoded);
    ASSERT_EQ(decode_str(encode_str(decoded)), decoded);
  }
}

static inline const std::pair<std::string,std::string> decodedEncodedOpaque [] {
  {"",""},
  {"authz=Bearer token","authz=Bearer%20token"},
  {"test=test1&authz=Bearer token","test=test1&authz=Bearer%20token"},
  {"test=","test="},
  {"test=&authz=Bearer token&authz2=[]","test=&authz=Bearer%20token&authz2=%5B%5D"}
};

TEST(XrdHttpTests,encodeOpaqueTest) {
  for(auto [decoded,encoded]: decodedEncodedOpaque) {
    ASSERT_EQ(encoded,encode_opaque(decoded));
  }
}

static inline const std::pair<std::string, std::map<std::string,std::string>> reprDigest[] {
  {"", {}},
  {"adler=:NVTD2Q==:",{{"adler","3554c3d9"}}},
  {"ADLER=:NVTD2Q==:,sha256=:vykSZ1cmY46OIQzsSuyJkkMG8mxfR1pq+KC05AntFds=:",{{"adler","3554c3d9"},{"sha256","bf2912675726638e8e210cec4aec89924306f26c5f475a6af8a0b4e409ed15db"}}},
  {"adler=",{}},
  {"adler=,sha256=:vykSZ1cmY46OIQzsSuyJkkMG8mxfR1pq+KC05AntFds=:",{{"sha256","bf2912675726638e8e210cec4aec89924306f26c5f475a6af8a0b4e409ed15db"}}},
  {"azerty",{}},
  {"=::value:",{}}
};

TEST(XrdHttpTests, parseReprDigest) {
  for(const auto & [input, expectedMap]: reprDigest) {
    std::map<std::string,std::string> output;
    XrdHttpHeaderUtils::parseReprDigest(input,output);
    ASSERT_EQ(expectedMap, output);
  }
}

TEST(XrdHttpTests, getCORSAllowOriginHeader) {
  std::unordered_set<std::string> allowedOrigins = {
    "https://helloworld.cern.ch",
    "https://anotherorigins.cern.ch"
  };
  XrdHttpCorsHandler corsHandler;
  for (const auto &allowedOrigin: allowedOrigins) {
    corsHandler.addAllowedOrigin(allowedOrigin);
  }
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader("test"));
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader(""));
  for (const auto &allowedOrigin: allowedOrigins) {
    std::string expected{"Access-Control-Allow-Origin: " + allowedOrigin};
    ASSERT_EQ(expected, corsHandler.getCORSAllowOriginHeader(allowedOrigin));
  }
  corsHandler.addAllowedOrigin("");
  corsHandler.addAllowedOrigin(" ");
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader(""));
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader(" "));
}

static inline const std::pair<std::string, std::map<std::string, uint8_t>> wantReprDigests[] {
  {"",{}},
  {"adler=1",{{"adler",1}}},
  {"AdLeR=1",{{"adler",1}}},
  {"adler=1, sha-512=2",{{"adler",1},{"sha-512",2}}},
  {"adler=",{}},
  {"adler=azerty",{}},
  {"adler=-1",{}},
  {"adler=11",{{"adler",10}}},
  {"=",{}},
};

TEST(XrdHttpTests, parseWantReprDigest) {
  for(const auto & [input, expectedMap]: wantReprDigests) {
    std::map<std::string,uint8_t> output;
    XrdHttpHeaderUtils::parseWantReprDigest(input,output);
    ASSERT_EQ(expectedMap, output);
  }
}

static inline const std::pair<std::string,std::pair<bool,std::vector<uint8_t>>> hexDigestToBinary [] {
  {"deadbeef", {true,std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}}},
  {"DEADBEEF", {true,std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}}},
  {"00",       {true,std::vector<uint8_t>{0x00}}},
  {"",         {true,std::vector<uint8_t>{}}},
  {"01ff",     {true,std::vector<uint8_t>{0x01, 0xff}}},
  {"xyz",      {false,{}}},  // Invalid input: 'x', 'y', 'z'
  {"1",        {false,{}}},  // Invalid input: odd length
};

TEST(XrdHttpTests, fromHexDigest) {
  for(const auto & [hex,expected]: hexDigestToBinary) {
    std::vector<uint8_t> outputBytes;
    ASSERT_EQ(expected.first,Fromhexdigest(hex,outputBytes));
    ASSERT_EQ(expected.second,outputBytes);
  }
}

static inline const std::pair<std::vector<uint8_t>,std::string> binaryToBase64[] {
  // Original test
  {{0xde, 0xad, 0xbe, 0xef}, "3q2+7w=="},
  // Empty vector --> empty Base64 string
  {{}, ""},
  // Single byte --> 2 padding characters
  {{0x4d}, "TQ=="},             // 'M'
  // Two bytes --> 1 padding character
  {{0x4d, 0x61}, "TWE="},       // 'Ma'
  // Three bytes --> no padding
  {{0x4d, 0x61, 0x6e}, "TWFu"}, // 'Man'
  // Null byte at start
  {{0x00, 0x01, 0x02}, "AAEC"},
  // Null byte in the middle
  {{0x41, 0x00, 0x42}, "QQBC"},
  // Full range from 0x00 to 0x0f
  {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
                             "AAECAwQFBgcICQoLDA0ODw=="},
  // ASCII "hello world"
  {{'h','e','l','l','o',' ','w','o','r','l','d'}, "aGVsbG8gd29ybGQ="},
  // All printable ASCII (truncated to fit in a short test)
  {{'A','B','C','D','E','F','G','H','I','J','K','L'}, "QUJDREVGR0hJSktM"}
};

TEST(XrdHttpTests,tobase64) {
  for(const auto & binToBase64: binaryToBase64) {
    std::string base64Output;
    Tobase64(binToBase64.first,base64Output);
    ASSERT_EQ(binToBase64.second,base64Output);
  }
}

TEST(XrdHttpTests,frombase64ToBytes) {
  for(const auto & binToBase64: binaryToBase64) {
    std::vector<uint8_t> output;
    base64ToBytes(binToBase64.second,output);
    ASSERT_EQ(binToBase64.first,output);
  }
}

static inline const std::pair<std::string,std::string> b64ToHex[] {
  {"",""},
  {"NVTD2Q==","3554c3d9"},
  {"+l8FpQ==","fa5f05a5"},
};

TEST(XrdHttpTests, base64DecodeEncodeHex) {
  for(const auto & b64ToH: b64ToHex) {
    std::string output;
    base64DecodeHex(b64ToH.first, output);
    ASSERT_EQ(b64ToH.second,output);
    std::vector<uint8_t> bytes;
    Fromhexdigest(output,bytes);
    Tobase64(bytes,output);
    ASSERT_EQ(b64ToH.first,output);
  }
}

//------------------------------------------------------------------------------
// Unit tests for the "XSHV" shovel framing protocol: hello encode/parse, data
// frame encode plus incremental stream decoding, and the timing-safe compare
// used for the hello token check.
//------------------------------------------------------------------------------

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstring>
#include <string>

#include "XrdApps/XrdMonCollect/XrdMonShovelFrame.hh"

#include <gtest/gtest.h>

namespace
{
// A sockaddr_in for a.b.c.d:port.
sockaddr_storage v4(const char* ip, uint16_t port, socklen_t& sl)
{
   sockaddr_storage ss; memset(&ss, 0, sizeof(ss));
   sockaddr_in* a = (sockaddr_in*)&ss;
   a->sin_family = AF_INET;
   a->sin_port   = htons(port);
   inet_pton(AF_INET, ip, &a->sin_addr);
   sl = sizeof(sockaddr_in);
   return ss;
}

// A sockaddr_in6 for [ip]:port (also takes IPv4-mapped literals).
sockaddr_storage v6(const char* ip, uint16_t port, socklen_t& sl)
{
   sockaddr_storage ss; memset(&ss, 0, sizeof(ss));
   sockaddr_in6* a = (sockaddr_in6*)&ss;
   a->sin6_family = AF_INET6;
   a->sin6_port   = htons(port);
   inet_pton(AF_INET6, ip, &a->sin6_addr);
   sl = sizeof(sockaddr_in6);
   return ss;
}
}

TEST(XrdMonShovelFrame, HelloRoundTrip)
{
   for (const std::string& tok : {std::string(),
                                 std::string("s3cret"),
                                 std::string(kShovelMaxToken, 'x')})
      {std::string h = XrdMonShovelHello(tok);
       ASSERT_EQ(h.size(), kShovelHelloFixed + tok.size());
       std::uint8_t ver = 0; std::string got;
       ASSERT_TRUE(XrdMonParseHello((const unsigned char*)h.data(), h.size(),
                                    ver, got));
       EXPECT_EQ(ver, kShovelVersion);
       EXPECT_EQ(got, tok);
      }
}

TEST(XrdMonShovelFrame, HelloMalformed)
{
   std::string h = XrdMonShovelHello("tok");
   std::uint8_t ver; std::string tok;

   // Too short for the fixed part.
   EXPECT_FALSE(XrdMonParseHello((const unsigned char*)h.data(), 7, ver, tok));

   // Bad magic.
   std::string bad = h; bad[0] = 'Y';
   EXPECT_FALSE(XrdMonParseHello((const unsigned char*)bad.data(), bad.size(),
                                 ver, tok));

   // Nonzero reserved flags.
   bad = h; bad[5] = 1;
   EXPECT_FALSE(XrdMonParseHello((const unsigned char*)bad.data(), bad.size(),
                                 ver, tok));

   // Length disagrees with the token length field.
   EXPECT_FALSE(XrdMonParseHello((const unsigned char*)h.data(), h.size() - 1,
                                 ver, tok));

   // Over-long token.
   std::uint16_t t = htons((std::uint16_t)(kShovelMaxToken + 1));
   bad = h; memcpy(&bad[6], &t, 2);
   bad.resize(kShovelHelloFixed + kShovelMaxToken + 1, 'x');
   EXPECT_FALSE(XrdMonParseHello((const unsigned char*)bad.data(), bad.size(),
                                 ver, tok));

   // A future version parses fine; accepting it is the caller's decision.
   std::string v2 = h; v2[4] = 2;
   ASSERT_TRUE(XrdMonParseHello((const unsigned char*)v2.data(), v2.size(),
                                ver, tok));
   EXPECT_EQ(ver, 2);
}

TEST(XrdMonShovelFrame, DataRoundTripV4)
{
   socklen_t sl;
   sockaddr_storage ss = v4("192.0.2.7", 9931, sl);
   const std::string payload = "f-stream bytes";

   std::string wire;
   ASSERT_TRUE(XrdMonShovelEncode(wire, (sockaddr*)&ss,
                                  payload.data(), payload.size()));

   XrdMonFrameReader r;
   XrdMonBatch b;
   ASSERT_TRUE(r.Consume(wire.data(), wire.size(), b));
   ASSERT_EQ(b.size(), 1u);
   EXPECT_EQ(b[0].data, payload);
   // The decoded src must be byte-identical to direct UDP reception.
   EXPECT_EQ(b[0].src, XrdMonSenderName((sockaddr*)&ss, sl));
   EXPECT_EQ(b[0].src, "192.0.2.7:9931");
   EXPECT_EQ(r.Frames(), 1u);
}

TEST(XrdMonShovelFrame, DataRoundTripV6AndMapped)
{
   socklen_t sl;
   for (const char* ip : {"2001:db8::1", "::ffff:127.0.0.1"})
      {sockaddr_storage ss = v6(ip, 1094, sl);
       std::string wire;
       ASSERT_TRUE(XrdMonShovelEncode(wire, (sockaddr*)&ss, "x", 1));

       XrdMonFrameReader r;
       XrdMonBatch b;
       ASSERT_TRUE(r.Consume(wire.data(), wire.size(), b));
       ASSERT_EQ(b.size(), 1u);
       EXPECT_EQ(b[0].src, XrdMonSenderName((sockaddr*)&ss, sl));
      }
}

TEST(XrdMonShovelFrame, IncrementalAndBatchedDecoding)
{
   socklen_t sl;
   sockaddr_storage a = v4("10.0.0.1", 1000, sl);
   sockaddr_storage b6 = v6("2001:db8::2", 2000, sl);

   std::string wire;
   ASSERT_TRUE(XrdMonShovelEncode(wire, (sockaddr*)&a,  "one", 3));
   ASSERT_TRUE(XrdMonShovelEncode(wire, (sockaddr*)&b6, "two2", 4));
   ASSERT_TRUE(XrdMonShovelEncode(wire, (sockaddr*)&a,  "three33", 7));

   // Fed one byte at a time, the reader must produce the same three packets.
   XrdMonFrameReader r;
   XrdMonBatch got;
   for (std::size_t i = 0; i < wire.size(); i++)
       ASSERT_TRUE(r.Consume(wire.data() + i, 1, got));
   ASSERT_EQ(got.size(), 3u);
   EXPECT_EQ(got[0].data, "one");
   EXPECT_EQ(got[0].src, "10.0.0.1:1000");
   EXPECT_EQ(got[1].data, "two2");
   EXPECT_EQ(got[2].data, "three33");

   // And in one gulp.
   XrdMonFrameReader r2;
   XrdMonBatch all;
   ASSERT_TRUE(r2.Consume(wire.data(), wire.size(), all));
   EXPECT_EQ(all.size(), 3u);

   // And split at an arbitrary mid-frame boundary.
   XrdMonFrameReader r3;
   XrdMonBatch parts;
   std::size_t cut = wire.size() / 2;
   ASSERT_TRUE(r3.Consume(wire.data(), cut, parts));
   ASSERT_TRUE(r3.Consume(wire.data() + cut, wire.size() - cut, parts));
   EXPECT_EQ(parts.size(), 3u);
}

TEST(XrdMonShovelFrame, EncodeRejectsBadInput)
{
   socklen_t sl;
   sockaddr_storage ss = v4("10.0.0.1", 1, sl);
   std::string wire;

   EXPECT_FALSE(XrdMonShovelEncode(wire, (sockaddr*)&ss, "", 0));
   std::string big(kShovelMaxPayload + 1, 'x');
   EXPECT_FALSE(XrdMonShovelEncode(wire, (sockaddr*)&ss,
                                   big.data(), big.size()));
   sockaddr un; memset(&un, 0, sizeof(un)); un.sa_family = AF_UNIX;
   EXPECT_FALSE(XrdMonShovelEncode(wire, &un, "x", 1));
   EXPECT_TRUE(wire.empty());              // nothing was appended
}

TEST(XrdMonShovelFrame, DecodeRejectsProtocolViolations)
{
   socklen_t sl;
   sockaddr_storage ss = v4("10.0.0.1", 1, sl);
   std::string good;
   ASSERT_TRUE(XrdMonShovelEncode(good, (sockaddr*)&ss, "x", 1));

   // Unknown frame type.
   {std::string w = good; w[0] = 0x02;
    XrdMonFrameReader r; XrdMonBatch b;
    EXPECT_FALSE(r.Consume(w.data(), w.size(), b));
   }
   // Unknown address family.
   {std::string w = good; w[1] = 5;
    XrdMonFrameReader r; XrdMonBatch b;
    EXPECT_FALSE(r.Consume(w.data(), w.size(), b));
   }
   // Zero-length payload.
   {std::string w = good;
    std::uint32_t l = htonl(0); memcpy(&w[8], &l, 4);
    XrdMonFrameReader r; XrdMonBatch b;
    EXPECT_FALSE(r.Consume(w.data(), w.size(), b));
   }
   // Oversized payload length.
   {std::string w = good;
    std::uint32_t l = htonl(kShovelMaxPayload + 1); memcpy(&w[8], &l, 4);
    XrdMonFrameReader r; XrdMonBatch b;
    EXPECT_FALSE(r.Consume(w.data(), w.size(), b));
   }
   // A truncated frame is not an error (more bytes may arrive), but produces
   // no packet.
   {XrdMonFrameReader r; XrdMonBatch b;
    EXPECT_TRUE(r.Consume(good.data(), good.size() - 1, b));
    EXPECT_TRUE(b.empty());
   }
}

TEST(XrdMonShovelFrame, TimingSafeEqual)
{
   EXPECT_TRUE (XrdMonTimingSafeEqual("", ""));
   EXPECT_TRUE (XrdMonTimingSafeEqual("secret", "secret"));
   EXPECT_FALSE(XrdMonTimingSafeEqual("secret", "secreT"));
   EXPECT_FALSE(XrdMonTimingSafeEqual("secret", "secre"));
   EXPECT_FALSE(XrdMonTimingSafeEqual("", "x"));
}

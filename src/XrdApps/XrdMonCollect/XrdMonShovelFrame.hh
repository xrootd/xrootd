#ifndef __XRDMONSHOVELFRAME_HH__
#define __XRDMONSHOVELFRAME_HH__
/******************************************************************************/
/*                                                                            */
/*                X r d M o n S h o v e l F r a m e . h h                     */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/******************************************************************************/

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>
#include <deque>
#include <string>

//-----------------------------------------------------------------------------
//! The "XSHV" shovel framing protocol (version 1): UDP monitoring datagrams
//! encapsulated over a TCP stream, so a shoveler running next to the packet
//! source can relay them losslessly to a remote collector. Each data frame
//! carries the datagram's ORIGINAL source address, which the collector uses to
//! attribute the packet to the emitting daemon (per-server incarnation state
//! and pseq-gap loss estimation are keyed on it).
//!
//! All multi-byte integers are in network byte order. TCP provides a reliable
//! byte stream, so there is no per-frame resync marker: any malformed field is
//! a protocol violation and the connection is dropped (the shoveler reconnects
//! and replays from its spool).
//!
//! Hello frame (exactly once, shoveler -> collector, right after connect):
//!
//!   offset  size  field
//!        0     4  magic "XSHV"
//!        4     1  protocol version (1)
//!        5     1  flags (reserved, must be 0)
//!        6     2  auth token length T (max 4096)
//!        8     T  auth token bytes (may be empty)
//!
//! Hello reply (collector -> shoveler, 1 byte): 0x01 accepted, 0x00 rejected
//! (bad magic/version/token), after which the collector closes the connection.
//!
//! Data frame (repeated):
//!
//!   offset  size  field
//!        0     1  frame type: 0x01 = data (others reserved)
//!        1     1  address family: 4 or 6
//!        2     2  original UDP source port
//!        4  4|16  original source address (in_addr / in6_addr)
//!        -     4  payload length L (1 <= L <= 65536)
//!        -     L  payload: one raw UDP monitoring datagram
//-----------------------------------------------------------------------------

constexpr char          kShovelMagic[4]   = {'X','S','H','V'};
constexpr std::uint8_t  kShovelVersion    = 1;
constexpr std::size_t   kShovelHelloFixed = 8;         // bytes before the token
constexpr std::size_t   kShovelMaxToken   = 4096;
constexpr std::uint8_t  kShovelAccept     = 0x01;      // hello reply codes
constexpr std::uint8_t  kShovelReject     = 0x00;
constexpr std::uint8_t  kShovelFrameData  = 0x01;
constexpr std::uint32_t kShovelMaxPayload = 64 * 1024; // a UDP datagram

//-----------------------------------------------------------------------------
//! One received datagram: the sender's numeric host:port, the raw bytes, and
//! (for the shoveler path, which must re-encode the sender on the wire) the
//! raw source address. The receiver thread fills these and hands batches of
//! them to the serializer (collector) or the shovel sender (shoveler).
//-----------------------------------------------------------------------------

struct XrdMonPacket
{
   std::string      src;          // numeric "host:port" of the original sender
   std::string      data;         // raw datagram bytes
   sockaddr_storage addr{};       // raw source address (shoveler path)
   socklen_t        addrLen = 0;
};

using XrdMonBatch = std::deque<XrdMonPacket>;

//! Numeric host:port string for a datagram sender (no DNS lookup).
//
inline std::string XrdMonSenderName(const sockaddr* sa, socklen_t sl)
{
   char host[NI_MAXHOST], serv[NI_MAXSERV];
   if (getnameinfo(sa, sl, host, sizeof(host), serv, sizeof(serv),
                   NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      return "unknown";
   std::string s = host;
   s += ':';
   s += serv;
   return s;
}

//! Constant-time string comparison — resists timing side-channels on secret
//! comparison (used for the hello token check).
//
inline bool XrdMonTimingSafeEqual(const std::string& a, const std::string& b)
{
   if (a.size() != b.size()) return false;
   unsigned char diff = 0;
   for (std::size_t i = 0; i < a.size(); i++)
       diff |= (unsigned char)(a[i] ^ b[i]);
   return diff == 0;
}

//! Encode the hello frame for `token` (which must be <= kShovelMaxToken bytes;
//! the caller validates that at startup).
//
inline std::string XrdMonShovelHello(const std::string& token)
{
   std::string out(kShovelMagic, sizeof(kShovelMagic));
   out.push_back((char)kShovelVersion);
   out.push_back(0);                                   // flags
   std::uint16_t t = htons((std::uint16_t)token.size());
   out.append((const char*)&t, 2);
   out += token;
   return out;
}

//! Parse a complete hello frame of `n` bytes. Returns true with the version
//! and token extracted; false on a malformed hello (bad magic, nonzero flags,
//! over-long token, or a length that does not match the token length field).
//! Accepting or rejecting the version is the caller's decision.
//
inline bool XrdMonParseHello(const unsigned char* buf, std::size_t n,
                             std::uint8_t& version, std::string& token)
{
   if (n < kShovelHelloFixed) return false;
   if (memcmp(buf, kShovelMagic, sizeof(kShovelMagic)) != 0) return false;
   version = buf[4];
   if (buf[5] != 0) return false;                      // reserved flags
   std::uint16_t t; memcpy(&t, buf + 6, 2); t = ntohs(t);
   if (t > kShovelMaxToken || n != kShovelHelloFixed + t) return false;
   token.assign((const char*)buf + kShovelHelloFixed, t);
   return true;
}

//! Append one data frame for a datagram of `len` bytes received from `sa` to
//! `out`. Returns false (nothing appended) for an unsupported address family
//! or an out-of-range payload.
//
inline bool XrdMonShovelEncode(std::string& out, const sockaddr* sa,
                               const char* payload, std::size_t len)
{
   if (!sa || len < 1 || len > kShovelMaxPayload) return false;

   const void*   ab;
   std::size_t   alen;
   std::uint16_t portN;                                // already network order
   std::uint8_t  family;
   if (sa->sa_family == AF_INET)
      {const sockaddr_in* a4 = (const sockaddr_in*)sa;
       family = 4; portN = a4->sin_port; ab = &a4->sin_addr; alen = 4;
      }
   else if (sa->sa_family == AF_INET6)
      {const sockaddr_in6* a6 = (const sockaddr_in6*)sa;
       family = 6; portN = a6->sin6_port; ab = &a6->sin6_addr; alen = 16;
      }
   else return false;

   out.push_back((char)kShovelFrameData);
   out.push_back((char)family);
   out.append((const char*)&portN, 2);
   out.append((const char*)ab, alen);
   std::uint32_t l = htonl((std::uint32_t)len);
   out.append((const char*)&l, 4);
   out.append(payload, len);
   return true;
}

//-----------------------------------------------------------------------------
//! Incremental data-frame parser for one connection: feed it whatever recv()
//! returned and it appends every complete decoded datagram to a batch, keeping
//! any trailing partial frame for the next call. The source address is
//! reconstructed into a sockaddr and formatted with XrdMonSenderName, so the
//! decoded `src` is byte-identical to what direct UDP reception would produce.
//-----------------------------------------------------------------------------

class XrdMonFrameReader
{
public:

   //! Consume `n` received bytes, appending decoded packets to `out`. Returns
   //! false on a protocol violation (the caller should drop the connection).
   bool Consume(const char* buf, std::size_t n, XrdMonBatch& out)
   {
      pend.append(buf, n);
      std::size_t off = 0;
      while (true)
           {std::size_t avail = pend.size() - off;
            if (avail < 4) break;
            const unsigned char* p = (const unsigned char*)pend.data() + off;
            if (p[0] != kShovelFrameData) return false;
            if (p[1] != 4 && p[1] != 6)   return false;
            std::size_t alen = (p[1] == 4) ? 4 : 16;
            std::size_t head = 4 + alen + 4;
            if (avail < head) break;
            std::uint32_t l; memcpy(&l, p + 4 + alen, 4); l = ntohl(l);
            if (l < 1 || l > kShovelMaxPayload) return false;
            if (avail < head + l) break;

            sockaddr_storage ss; memset(&ss, 0, sizeof(ss));
            socklen_t sl;
            if (p[1] == 4)
               {sockaddr_in* a4 = (sockaddr_in*)&ss;
                a4->sin_family = AF_INET;
                memcpy(&a4->sin_port, p + 2, 2);
                memcpy(&a4->sin_addr, p + 4, 4);
                sl = sizeof(sockaddr_in);
               }
               else
               {sockaddr_in6* a6 = (sockaddr_in6*)&ss;
                a6->sin6_family = AF_INET6;
                memcpy(&a6->sin6_port, p + 2, 2);
                memcpy(&a6->sin6_addr, p + 4, 16);
                sl = sizeof(sockaddr_in6);
               }

            XrdMonPacket pkt;
            pkt.src  = XrdMonSenderName((sockaddr*)&ss, sl);
            pkt.data.assign((const char*)p + head, l);
            pkt.addr = ss;
            pkt.addrLen = sl;
            out.push_back(std::move(pkt));
            nFrames++;
            off += head + l;
           }
      pend.erase(0, off);
      return true;
   }

   std::uint64_t Frames() const { return nFrames; }

private:
   std::string   pend;            // bytes of a trailing partial frame
   std::uint64_t nFrames = 0;
};

#endif

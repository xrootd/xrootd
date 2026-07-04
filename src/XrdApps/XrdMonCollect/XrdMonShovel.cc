/******************************************************************************/
/*                                                                            */
/*                     X r d M o n S h o v e l . c c                          */
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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "XrdApps/XrdMonCollect/XrdMonNet.hh"
#include "XrdApps/XrdMonCollect/XrdMonShovel.hh"
#include "XrdApps/XrdMonCollect/XrdMonShovelFrame.hh"

namespace
{
const int kRetryCooldown  = 5;   // seconds between reconnect attempts
const int kRejectCooldown = 30;  // after an explicit hello rejection
const int kConnectTimeout = 2;   // seconds to wait for a TCP connect
const int kSendTimeout    = 2;   // seconds a send() may block (SO_SNDTIMEO)
const int kReplyTimeout   = 5;   // seconds to wait for the hello verdict
}

XrdMonShovel::~XrdMonShovel()
{
   int s = fd.load();
   if (s >= 0) close(s);
}

void XrdMonShovel::Drop(int cooldown)
{
   int s = fd.exchange(-1);
   if (s >= 0) close(s);
   nextTry = time(0) + cooldown;
}

bool XrdMonShovel::Connect(std::string& err)
{
// Honor the cool-down so an unreachable collector is not retried per buffer.
//
   if (fd.load() >= 0) return true;
   if (time(0) < nextTry) {err = "collector unreachable (cooling down)";
                           return false;}

   addrinfo hints; memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   char portStr[16]; snprintf(portStr, sizeof(portStr), "%d", port);

   addrinfo* res = nullptr;
   if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res)
      {err = "cannot resolve collector host '" + host + "'";
       Drop(kRetryCooldown); return false;}

   int s = -1;
   for (addrinfo* ai = res; ai; ai = ai->ai_next)
       {s = XrdMonNet::connectTimed(ai, kConnectTimeout, kSendTimeout);
        if (s >= 0) break;
       }
   freeaddrinfo(res);

   if (s < 0) {err = "cannot connect to collector "
                   + host + ":" + portStr + ": " + strerror(errno);
               Drop(kRetryCooldown); return false;}

// Handshake: send the hello and await the collector's 1-byte verdict. An
// explicit rejection means a token or protocol-version mismatch — a
// configuration error — so back off much longer than for a network failure.
//
   const std::string hello = XrdMonShovelHello(token);
   size_t off = 0;
   while (off < hello.size())
        {ssize_t n = send(s, hello.data() + off, hello.size() - off,
                          MSG_NOSIGNAL);
         if (n > 0) {off += (size_t)n; continue;}
         if (n < 0 && errno == EINTR) continue;
         close(s);
         err = "cannot send hello to collector: "
             + std::string(n < 0 ? strerror(errno) : "connection closed");
         nextTry = time(0) + kRetryCooldown;
         return false;
        }

   struct timeval tv; tv.tv_sec = kReplyTimeout; tv.tv_usec = 0;
   setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   unsigned char verdict = kShovelReject;
   ssize_t n;
   do {n = recv(s, &verdict, 1, 0);} while (n < 0 && errno == EINTR);
   if (n != 1)
      {close(s);
       err = "no hello reply from collector";
       nextTry = time(0) + kRetryCooldown;
       return false;
      }
   if (verdict != kShovelAccept)
      {close(s);
       err = "rejected by collector (token/version mismatch)";
       nextTry = time(0) + kRejectCooldown;
       return false;
      }

   fd.store(s);
   nConnects++;
   return true;
}

bool XrdMonShovel::Send(const std::string& buf, std::string& err)
{
   if (!Connect(err)) return false;

// Write the whole buffer, tolerating short writes. Any error tears the
// connection down; the next call reconnects (after the cool-down).
//
   int s = fd.load();
   size_t off = 0;
   while (off < buf.size())
        {ssize_t n = send(s, buf.data() + off, buf.size() - off, MSG_NOSIGNAL);
         if (n > 0) {off += (size_t)n; continue;}
         if (n < 0 && errno == EINTR) continue;
         err = "shovel send failed: "
             + std::string(n < 0 ? strerror(errno) : "connection closed");
         Drop(kRetryCooldown);
         return false;
        }
   return true;
}

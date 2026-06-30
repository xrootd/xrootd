/******************************************************************************/
/*                                                                            */
/*                    X r d M o n F o r w a r d . c c                         */
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
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdApps/XrdMonCollect/XrdMonForward.hh"

namespace
{
const int kRetryCooldown   = 5;   // seconds between reconnect attempts
const int kConnectTimeout  = 2;   // seconds to wait for a TCP connect
const int kSendTimeout     = 2;   // seconds a send() may block (SO_SNDTIMEO)

// connect() with a bounded wait, so an unresponsive consumer cannot stall the
// caller (the serializer thread) indefinitely. Returns a connected, blocking
// socket with SO_SNDTIMEO set, or -1.
int connectTimed(const addrinfo* ai)
{
   int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
   if (s < 0) return -1;

   int fl = fcntl(s, F_GETFL, 0);
   fcntl(s, F_SETFL, fl | O_NONBLOCK);
   int rc = connect(s, ai->ai_addr, ai->ai_addrlen);
   if (rc != 0)
      {if (errno != EINPROGRESS) {close(s); return -1;}
       struct pollfd pfd; pfd.fd = s; pfd.events = POLLOUT; pfd.revents = 0;
       if (poll(&pfd, 1, kConnectTimeout * 1000) <= 0) {close(s); return -1;}
       int soerr = 0; socklen_t l = sizeof(soerr);
       if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &l) != 0 || soerr != 0)
          {close(s); return -1;}
      }
   fcntl(s, F_SETFL, fl);                 // restore blocking mode

   struct timeval tv; tv.tv_sec = kSendTimeout; tv.tv_usec = 0;
   setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
   return s;
}
}

XrdMonForward::~XrdMonForward()
{
   if (fd >= 0) close(fd);
}

void XrdMonForward::Drop()
{
   if (fd >= 0) {close(fd); fd = -1;}
   nextTry = time(0) + kRetryCooldown;
}

bool XrdMonForward::Connect(std::string& err)
{
// Honor the cool-down so an unreachable consumer is not retried per packet.
//
   if (fd >= 0) return true;
   if (time(0) < nextTry) {err = "forward consumer unreachable (cooling down)";
                           return false;}

   addrinfo hints; memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   char portStr[16]; snprintf(portStr, sizeof(portStr), "%d", port);

   addrinfo* res = nullptr;
   if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res)
      {err = "cannot resolve forward host '" + host + "'"; Drop(); return false;}

   int s = -1;
   for (addrinfo* ai = res; ai; ai = ai->ai_next)
       {s = connectTimed(ai);
        if (s >= 0) break;
       }
   freeaddrinfo(res);

   if (s < 0) {err = "cannot connect to forward consumer "
                   + host + ":" + portStr + ": " + strerror(errno);
               Drop(); return false;}
   fd = s;
   return true;
}

bool XrdMonForward::Send(const std::string& jsonDoc, std::string& err)
{
   if (!Connect(err)) return false;

   std::string line = jsonDoc;
   line += '\n';

// Write the whole line, tolerating short writes. Any error tears the
// connection down; the next call reconnects (after the cool-down).
//
   size_t off = 0;
   while(off < line.size())
        {ssize_t n = send(fd, line.data() + off, line.size() - off,
                          MSG_NOSIGNAL);
         if (n > 0) {off += (size_t)n; continue;}
         if (n < 0 && errno == EINTR) continue;
         err = "forward send failed: "
             + std::string(n < 0 ? strerror(errno) : "connection closed");
         Drop();
         return false;
        }
   return true;
}

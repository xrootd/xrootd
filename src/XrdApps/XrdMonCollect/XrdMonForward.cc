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
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdApps/XrdMonCollect/XrdMonForward.hh"
#include "XrdApps/XrdMonCollect/XrdMonNet.hh"

namespace
{
const int kRetryCooldown   = 5;   // seconds between reconnect attempts
const int kConnectTimeout  = 2;   // seconds to wait for a TCP connect
const int kSendTimeout     = 2;   // seconds a send() may block (SO_SNDTIMEO)
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
       {s = XrdMonNet::connectTimed(ai, kConnectTimeout, kSendTimeout);
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

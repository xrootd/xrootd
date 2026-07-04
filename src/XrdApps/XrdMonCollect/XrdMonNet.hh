#ifndef __XRDMONNET_HH__
#define __XRDMONNET_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d M o n N e t . h h                             */
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
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace XrdMonNet
{
// connect() with a bounded wait, so an unresponsive consumer cannot stall the
// caller indefinitely. Returns a connected, blocking socket with SO_SNDTIMEO
// set to sendSecs, or -1. Shared by the NDJSON forward and shovel senders.
//
inline int connectTimed(const addrinfo* ai, int connectSecs, int sendSecs)
{
   int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
   if (s < 0) return -1;

   int fl = fcntl(s, F_GETFL, 0);
   fcntl(s, F_SETFL, fl | O_NONBLOCK);
   int rc = connect(s, ai->ai_addr, ai->ai_addrlen);
   if (rc != 0)
      {if (errno != EINPROGRESS) {close(s); return -1;}
       struct pollfd pfd; pfd.fd = s; pfd.events = POLLOUT; pfd.revents = 0;
       if (poll(&pfd, 1, connectSecs * 1000) <= 0) {close(s); return -1;}
       int soerr = 0; socklen_t l = sizeof(soerr);
       if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &l) != 0 || soerr != 0)
          {close(s); return -1;}
      }
   fcntl(s, F_SETFL, fl);                 // restore blocking mode

   struct timeval tv; tv.tv_sec = sendSecs; tv.tv_usec = 0;
   setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
   return s;
}
}
#endif

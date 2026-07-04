/******************************************************************************/
/*                                                                            */
/*                  X r d M o n T c p S e r v e r . c c                       */
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

#include "XrdApps/XrdMonCollect/XrdMonTcpServer.hh"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace
{
const int kHelloTimeout  = 5;   // seconds to wait for the hello (slowloris guard)
const int kIdleTimeout   = 1;   // recv timeout so threads notice Stop()
const int kListenBacklog = 64;
const int kMaxConns      = 512; // hard cap on concurrent connections

void setRecvTimeout(int fd, int secs)
{
   struct timeval tv; tv.tv_sec = secs; tv.tv_usec = 0;
   setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// Read exactly n bytes. False on EOF, error, or an SO_RCVTIMEO timeout.
//
bool readFully(int fd, void* buf, std::size_t n)
{
   char* p = (char*)buf;
   while (n > 0)
        {ssize_t r = recv(fd, p, n, 0);
         if (r > 0) {p += r; n -= (std::size_t)r; continue;}
         if (r < 0 && errno == EINTR) continue;
         return false;
        }
   return true;
}
}

bool XrdMonTcpServer::Start(std::string& err)
{
// A pre-bound listener (systemd socket activation) is already bound and
// listening; there is nothing to create. It also survives service restarts,
// so pending shovel connections just queue in its backlog while we are down.
//
   if (inheritedFd >= 0)
      {listenFd = inheritedFd;
       setRecvTimeout(listenFd, 1);       // accept() wakes to check stopping
       accepter = std::thread(&XrdMonTcpServer::acceptLoop, this);
       return true;
      }

// Create the listen socket: dual-stack IPv6 by default (like the UDP socket),
// or the family of an explicit bind address.
//
   if (!bindStr.empty())
      {addrinfo hints; memset(&hints, 0, sizeof(hints));
       hints.ai_family   = AF_UNSPEC;
       hints.ai_socktype = SOCK_STREAM;
       hints.ai_flags    = AI_NUMERICHOST | AI_PASSIVE;
       char portStr[16]; snprintf(portStr, sizeof(portStr), "%d", port);
       addrinfo* res = nullptr;
       if (getaddrinfo(bindStr.c_str(), portStr, &hints, &res) != 0 || !res)
          {err = "invalid bind address '" + bindStr + "'"; return false;}
       listenFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
       int one = 1;
       if (listenFd >= 0)
          setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
       if (listenFd >= 0 && bind(listenFd, res->ai_addr, res->ai_addrlen) < 0)
          {close(listenFd); listenFd = -1;}
       freeaddrinfo(res);
      }
      else
      {listenFd = socket(AF_INET6, SOCK_STREAM, 0);
       if (listenFd >= 0)
          {int one = 1, off = 0;
           setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
           setsockopt(listenFd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
           sockaddr_in6 me; memset(&me, 0, sizeof(me));
           me.sin6_family = AF_INET6;
           me.sin6_port   = htons((uint16_t)port);
           me.sin6_addr   = in6addr_any;
           if (bind(listenFd, (sockaddr*)&me, sizeof(me)) < 0)
              {close(listenFd); listenFd = -1;}
          }
      }
   if (listenFd < 0 || listen(listenFd, kListenBacklog) < 0)
      {err = "cannot listen on TCP port " + std::to_string(port) + ": "
           + strerror(errno);
       if (listenFd >= 0) {close(listenFd); listenFd = -1;}
       return false;
      }

   setRecvTimeout(listenFd, 1);           // accept() wakes to check stopping
   accepter = std::thread(&XrdMonTcpServer::acceptLoop, this);
   return true;
}

void XrdMonTcpServer::Stop()
{
   if (stopping.exchange(true)) return;   // already stopped

   if (listenFd >= 0) shutdown(listenFd, SHUT_RDWR);
   if (accepter.joinable()) accepter.join();
   if (listenFd >= 0) {close(listenFd); listenFd = -1;}

// Unblock every connection thread's recv(), then join them all. A thread
// removes itself from `conns` and parks its handle in `done` as it exits.
//
   {std::lock_guard<std::mutex> lk(connMtx);
    for (auto& kv : conns) shutdown(kv.first, SHUT_RDWR);
   }
   for (;;)
      {std::thread t;
       bool empty;
       {std::lock_guard<std::mutex> lk(connMtx);
        if (!done.empty()) {t = std::move(done.front()); done.pop_front();}
        empty = done.empty() && conns.empty();
       }
       if (t.joinable()) {t.join(); continue;}
       if (empty) break;
       usleep(10 * 1000);                 // a thread is still winding down
      }
}

void XrdMonTcpServer::acceptLoop()
{
   while (!stopping)
        {// Reap connection threads that have finished.
         for (;;)
            {std::thread t;
             {std::lock_guard<std::mutex> lk(connMtx);
              if (done.empty()) break;
              t = std::move(done.front()); done.pop_front();
             }
             t.join();
            }

         int c = accept(listenFd, nullptr, nullptr);
         if (c < 0)
            {if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
             if (!stopping) perror("xrdmoncollect: tcp accept");
             break;
            }
         if (stopping) {close(c); break;}
         if (nActive.load() >= kMaxConns) {close(c); continue;}

         // Insert under the lock BEFORE the new thread can finish, so its
         // self-cleanup always finds the map entry.
         std::lock_guard<std::mutex> lk(connMtx);
         conns.emplace(c, std::thread(&XrdMonTcpServer::serveConn, this, c));
        }
}

void XrdMonTcpServer::serveConn(int fd)
{
   nConns++;
   nActive++;

// Hello: fixed part, then the token, all within the hello timeout. A timeout,
// a bad magic/version, or an over-long token is a protocol violation; only a
// well-formed hello with the wrong token is an auth failure.
//
   bool authorized = false;
   setRecvTimeout(fd, kHelloTimeout);
   unsigned char fixed[kShovelHelloFixed];
   if (readFully(fd, fixed, sizeof(fixed)))
      {std::uint16_t t; memcpy(&t, fixed + 6, 2); t = ntohs(t);
       std::string hello((const char*)fixed, sizeof(fixed));
       bool ok = t <= kShovelMaxToken;
       if (ok && t)
          {hello.resize(sizeof(fixed) + t);
           ok = readFully(fd, &hello[sizeof(fixed)], t);
          }
       std::uint8_t version = 0; std::string tok;
       if (ok) ok = XrdMonParseHello((const unsigned char*)hello.data(),
                                     hello.size(), version, tok)
                    && version == kShovelVersion;
       if (!ok) nMalformed++;
          else if (!token.empty() && !XrdMonTimingSafeEqual(token, tok))
          nAuthFail++;
          else authorized = true;
      }
      else nMalformed++;

   unsigned char reply = authorized ? kShovelAccept : kShovelReject;
   if (send(fd, &reply, 1, MSG_NOSIGNAL) != 1) authorized = false;

   if (authorized)
      {setRecvTimeout(fd, kIdleTimeout);
       XrdMonFrameReader reader;
       XrdMonBatch cur;                   // local accumulation (not pipe-owned,
                                          // so an idle connection holds no
                                          // pipe buffer hostage)
       // Hand the accumulated packets to the serializer through the shared
       // receive pipe. False once the pipe is closed (shutdown).
       auto flushBatch = [&]() -> bool
          {XrdMonBatch b;
           if (!pipe.acquire(b)) return false;
           std::swap(b, cur);
           pipe.submit(std::move(b));
           return true;
          };

       time_t batchStart = time(0);
       char   buff[64 * 1024];
       while (!stopping)
            {ssize_t n = recv(fd, buff, sizeof(buff), 0);
             if (n < 0)
                {if (errno == EINTR) continue;
                 if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {if (!cur.empty() && time(0) - batchStart >= flushSecs)
                        {if (!flushBatch()) break;
                         batchStart = time(0);
                        }
                     continue;
                    }
                 break;                   // connection error
                }
             if (n == 0) break;           // EOF: shoveler went away
             nBytes += (std::uint64_t)n;
             std::size_t before = cur.size();
             if (!reader.Consume(buff, (std::size_t)n, cur))
                {nMalformed++; break;}    // protocol violation: drop connection
             nFrames += cur.size() - before;
             if (cur.size() >= flushCount ||
                 (!cur.empty() && time(0) - batchStart >= flushSecs))
                {if (!flushBatch()) break;
                 batchStart = time(0);
                }
            }
       if (!cur.empty()) flushBatch();    // hand off any partial batch
      }

// Self-cleanup: leave the map first (so Stop() cannot shutdown() a recycled
// fd number), then close, then park the thread handle for joining.
//
   {std::lock_guard<std::mutex> lk(connMtx);
    auto it = conns.find(fd);
    if (it != conns.end())
       {done.push_back(std::move(it->second));
        conns.erase(it);
       }
   }
   close(fd);
   nActive--;
}

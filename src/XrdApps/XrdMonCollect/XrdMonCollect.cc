/******************************************************************************/
/*                                                                            */
/*                    X r d M o n C o l l e c t . c c                         */
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

// xrdmoncollect: read XRootD detailed-monitoring UDP packets (xrootd.monitor),
// correlate the "f" (file-stats) stream into one document per completed
// transfer, and write the documents as NDJSON (or OpenSearch _bulk) to stdout
// or a file. See xrootd-new-metrics.md, Phase 5.

#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"

namespace
{
volatile sig_atomic_t stopFlag = 0;
void onSignal(int) {stopFlag = 1;}

void usage(const char* prog)
{
   fprintf(stderr,
     "Usage: %s -p <port> [-b <bindaddr>] [-o <file>] [--bulk <index>]\n"
     "          [--dump] [-v]\n\n"
     "  -p <port>       UDP port to listen on (required)\n"
     "  -b <bindaddr>   IPv4 address to bind (default: all interfaces)\n"
     "  -o <file>       append output to <file> (default: stdout)\n"
     "  --bulk <index>  emit OpenSearch _bulk format for the given index\n"
     "  --dump          also emit one JSON object per decoded record\n"
     "  -v              print decoder statistics on exit\n", prog);
}

// Create and bind a UDP socket. With no bind address, an IPv6 dual-stack
// socket is used so packets from both IPv4 and IPv6 senders are received
// (servers commonly resolve "localhost" to ::1). With an explicit address the
// family is taken from it. Returns the fd, or -1 with a message on stderr.
//
int openUDP(int port, const char* bindStr)
{
   if (bindStr)
      {addrinfo hints; memset(&hints, 0, sizeof(hints));
       hints.ai_family   = AF_UNSPEC;
       hints.ai_socktype = SOCK_DGRAM;
       hints.ai_flags    = AI_NUMERICHOST | AI_PASSIVE;
       char portStr[16]; snprintf(portStr, sizeof(portStr), "%d", port);
       addrinfo* res = nullptr;
       if (getaddrinfo(bindStr, portStr, &hints, &res) != 0 || !res)
          {fprintf(stderr, "invalid bind address '%s'\n", bindStr); return -1;}
       int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
       if (fd >= 0 && bind(fd, res->ai_addr, res->ai_addrlen) < 0)
          {perror("bind"); close(fd); fd = -1;}
       freeaddrinfo(res);
       return fd;
      }

   int fd = socket(AF_INET6, SOCK_DGRAM, 0);
   if (fd < 0) {perror("socket"); return -1;}

   int off = 0;  // IPV6_V6ONLY off => also receive IPv4-mapped traffic
   setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

   sockaddr_in6 me; memset(&me, 0, sizeof(me));
   me.sin6_family = AF_INET6;
   me.sin6_port   = htons((uint16_t)port);
   me.sin6_addr   = in6addr_any;
   if (bind(fd, (sockaddr*)&me, sizeof(me)) < 0)
      {perror("bind"); close(fd); return -1;}
   return fd;
}

// Numeric host:port string for a datagram sender (no DNS lookup).
//
std::string senderName(const sockaddr* sa, socklen_t sl)
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
}

int main(int argc, char* argv[])
{
   int         port    = 0;
   const char* bindStr = nullptr;
   const char* outFile = nullptr;
   std::string bulkIdx;
   bool        dump    = false;
   bool        verbose = false;

// Parse arguments
//
   for (int i = 1; i < argc; i++)
       {const char* a = argv[i];
             if (!strcmp(a, "-p") && i+1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(a, "-b") && i+1 < argc) bindStr = argv[++i];
        else if (!strcmp(a, "-o") && i+1 < argc) outFile = argv[++i];
        else if (!strcmp(a, "--bulk") && i+1 < argc) bulkIdx = argv[++i];
        else if (!strcmp(a, "--dump")) dump = true;
        else if (!strcmp(a, "-v")) verbose = true;
        else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {usage(argv[0]); return 0;}
        else {fprintf(stderr, "%s: unknown argument '%s'\n", argv[0], a);
              usage(argv[0]); return 2;}
       }

   if (port <= 0 || port > 65535)
      {fprintf(stderr, "%s: a valid -p <port> is required\n", argv[0]);
       usage(argv[0]); return 2;}

// Open the output sink
//
   FILE* out = stdout;
   if (outFile && !(out = fopen(outFile, "a")))
      {fprintf(stderr, "%s: cannot open '%s': %s\n", argv[0], outFile,
               strerror(errno)); return 4;}

// Create and bind the UDP socket (dual-stack by default)
//
   int fd = openUDP(port, bindStr);
   if (fd < 0) return 4;

// Wire up the sinks. With --bulk each document is preceded by an index action
// line, producing a stream directly postable to the OpenSearch _bulk API.
//
   XrdMonDecode::DocSink docSink =
      [&](const std::string& d)
         {if (bulkIdx.empty()) fprintf(out, "%s\n", d.c_str());
             else fprintf(out, "{\"index\":{\"_index\":\"%s\"}}\n%s\n",
                          bulkIdx.c_str(), d.c_str());
         };

   XrdMonDecode::RawSink rawSink;
   if (dump) rawSink = [&](const std::string& r){fprintf(out, "%s\n", r.c_str());};

   XrdMonDecode decoder(docSink, rawSink, dump);

// Receive loop
//
// Install handlers without SA_RESTART so a signal interrupts the blocking
// recvfrom() (returns EINTR) and the loop can notice stopFlag and exit.
//
   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = onSignal;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sigaction(SIGINT,  &sa, nullptr);
   sigaction(SIGTERM, &sa, nullptr);

   char buff[64*1024];
   while(!stopFlag)
        {sockaddr_storage from;
         socklen_t fromLen = sizeof(from);
         ssize_t n = recvfrom(fd, buff, sizeof(buff), 0,
                              (sockaddr*)&from, &fromLen);
         if (n < 0) {if (errno == EINTR) continue; perror("recvfrom"); break;}

         decoder.Process(senderName((sockaddr*)&from, fromLen), buff, (int)n);
         fflush(out);
        }

   if (verbose)
      {const XrdMonDecode::Stats& s = decoder.GetStats();
       fprintf(stderr,
         "xrdmoncollect: packets=%llu malformed=%llu records=%llu "
         "mapUser=%llu opens=%llu closes=%llu docs=%llu orphanCloses=%llu "
         "unknown=%llu\n",
         (unsigned long long)s.packets, (unsigned long long)s.malformed,
         (unsigned long long)s.records, (unsigned long long)s.mapUser,
         (unsigned long long)s.opens, (unsigned long long)s.closes,
         (unsigned long long)s.docs, (unsigned long long)s.orphanCls,
         (unsigned long long)s.unknown);
      }

   if (out != stdout) fclose(out);
   close(fd);
   return 0;
}

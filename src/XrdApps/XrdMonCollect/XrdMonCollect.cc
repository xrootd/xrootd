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
#include <ctime>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <thread>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
#include "XrdApps/XrdMonCollect/XrdMonForward.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#ifdef XRDMON_HAVE_CURL
#include "XrdApps/XrdMonCollect/XrdMonOpenSearch.hh"
#endif

namespace
{
volatile sig_atomic_t stopFlag = 0;
void onSignal(int) {stopFlag = 1;}

void usage(const char* prog)
{
   fprintf(stderr,
     "Usage: %s -p <port> [-b <bindaddr>] [-o <file>] [--bulk <index>]\n"
     "          [--os-url <url> [--os-index <name>] [--os-user <u>]\n"
     "           [--os-pass <p>] [--os-insecure]]\n"
     "          [--flush-count <n>] [--flush-secs <n>] [--dump] [-v]\n\n"
     "  -p <port>        UDP port to listen on (required)\n"
     "  -b <bindaddr>    address to bind (default: all interfaces, dual-stack)\n"
     "  -o <file>        append output to <file> (default: stdout unless --os-url)\n"
     "  --bulk <index>   write OpenSearch _bulk format to the file/stdout sink\n"
     "  --os-url <url>   POST documents to an OpenSearch cluster's _bulk API\n"
     "  --os-index <n>   index/data-stream name (default: xrootd-transfers)\n"
     "  --os-user <u>    basic-auth user\n"
     "  --os-pass <p>    basic-auth password\n"
     "  --os-insecure    skip TLS certificate verification\n"
     "  --os-datastream  target is a data stream (use the \"create\" action)\n"
     "  --forward <h:p>  also stream documents as NDJSON over TCP to host:port\n"
     "                   (e.g. a logstash/fluentd/vector buffering frontend)\n"
     "  --flush-count <n> flush after N documents (default: 500)\n"
     "  --flush-secs <n>  flush after N seconds (default: 5)\n"
     "  --metrics-port <p> serve aggregated metrics over HTTP on port <p>\n"
     "  --max-entries <n> cap per-server dict/open-file entries (0=unbounded)\n"
     "  --scitags <file> SciTags registry JSON mapping experiment/activity ids\n"
     "                   to names (and a VO); numeric ids are kept either way\n"
     "  --no-resolve     do not substitute the local FQDN for a loopback server\n"
     "  --traces         emit a document per t-stream I/O record (high volume)\n"
     "  --gstream        emit a document per g-stream (plugin) record\n"
     "  --redirects      emit a document per r-stream redirect record\n"
     "  --dump           also emit one JSON object per decoded record\n"
     "  -v               print decoder statistics on exit\n", prog);
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

// A minimal HTTP exporter: serves the metrics registry to any GET request on
// the given TCP port. Prometheus scrapes /metrics; we answer every path.
//
// The collector's own metrics registry. Its prefix is empty so the explicit
// xrootd_collector_* metric names pass through unchanged.
//
static XrdMetrics::Registry& collectorRegistry()
{
   static XrdMetrics::Registry reg("");
   return reg;
}

void serveMetrics(int port, std::atomic<bool>& stop)
{
   int ls = socket(AF_INET, SOCK_STREAM, 0);
   if (ls < 0) {perror("metrics socket"); return;}
   int one = 1;
   setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

   sockaddr_in a; memset(&a, 0, sizeof(a));
   a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY;
   a.sin_port = htons((uint16_t)port);
   if (bind(ls, (sockaddr*)&a, sizeof(a)) < 0 || listen(ls, 8) < 0)
      {perror("metrics bind/listen"); close(ls); return;}

   timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;        // wake to check stop
   setsockopt(ls, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

   while(!stop)
        {int c = accept(ls, nullptr, nullptr);
         if (c < 0)
            {if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
             break;
            }
         char req[2048];
         recv(c, req, sizeof(req), 0);              // read & ignore the request

         std::string body;
         XrdMetrics::PrometheusTextSerializer ser(body);
         collectorRegistry().serialize(ser);
         collectorRegistry().runTextCollectors(body);
         std::string resp = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" + body;
         send(c, resp.data(), resp.size(), MSG_NOSIGNAL);
         close(c);
        }
   close(ls);
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
   bool        traces  = false;
   bool        gstream = false;
   bool        redirects = false;
   int         metricsPort = 0;
   size_t      maxEntries = 1000000;
   std::string osUrl, osUser, osPass;
   std::string osIndex = "xrootd-transfers";
   bool        osInsecure = false;
   bool        osDataStream = false;
   std::string fwdHost; int fwdPort = 0;
   size_t      flushCount = 500;
   long        flushSecs  = 5;
   std::string scitags;
   bool        resolve = true;

// Parse arguments
//
   for (int i = 1; i < argc; i++)
       {const char* a = argv[i];
             if (!strcmp(a, "-p") && i+1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(a, "-b") && i+1 < argc) bindStr = argv[++i];
        else if (!strcmp(a, "-o") && i+1 < argc) outFile = argv[++i];
        else if (!strcmp(a, "--bulk") && i+1 < argc) bulkIdx = argv[++i];
        else if (!strcmp(a, "--os-url") && i+1 < argc) osUrl = argv[++i];
        else if (!strcmp(a, "--os-index") && i+1 < argc) osIndex = argv[++i];
        else if (!strcmp(a, "--os-user") && i+1 < argc) osUser = argv[++i];
        else if (!strcmp(a, "--os-pass") && i+1 < argc) osPass = argv[++i];
        else if (!strcmp(a, "--os-insecure")) osInsecure = true;
        else if (!strcmp(a, "--os-datastream")) osDataStream = true;
        else if (!strcmp(a, "--forward") && i+1 < argc)
                {std::string hp = argv[++i];
                 auto c = hp.rfind(':');
                 if (c == std::string::npos || c == 0 || c+1 >= hp.size())
                    {fprintf(stderr, "%s: --forward needs host:port\n", argv[0]);
                     return 2;}
                 fwdHost = hp.substr(0, c);
                 fwdPort = atoi(hp.c_str() + c + 1);
                }
        else if (!strcmp(a, "--flush-count") && i+1 < argc) flushCount = (size_t)atol(argv[++i]);
        else if (!strcmp(a, "--flush-secs") && i+1 < argc) flushSecs = atol(argv[++i]);
        else if (!strcmp(a, "--metrics-port") && i+1 < argc) metricsPort = atoi(argv[++i]);
        else if (!strcmp(a, "--max-entries") && i+1 < argc) maxEntries = (size_t)atol(argv[++i]);
        else if (!strcmp(a, "--scitags") && i+1 < argc) scitags = argv[++i];
        else if (!strcmp(a, "--no-resolve")) resolve = false;
        else if (!strcmp(a, "--traces")) traces = true;
        else if (!strcmp(a, "--gstream")) gstream = true;
        else if (!strcmp(a, "--redirects")) redirects = true;
        else if (!strcmp(a, "--dump")) dump = true;
        else if (!strcmp(a, "-v")) verbose = true;
        else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {usage(argv[0]); return 0;}
        else {fprintf(stderr, "%s: unknown argument '%s'\n", argv[0], a);
              usage(argv[0]); return 2;}
       }

   if (port <= 0 || port > 65535)
      {fprintf(stderr, "%s: a valid -p <port> is required\n", argv[0]);
       usage(argv[0]); return 2;}

// Set up the OpenSearch sink if requested.
//
   bool osEnabled = false;
#ifdef XRDMON_HAVE_CURL
   XrdMonOpenSearch* os = nullptr;
#endif
   if (!osUrl.empty())
      {
#ifdef XRDMON_HAVE_CURL
       os = new XrdMonOpenSearch(osUrl, osIndex, osUser, osPass, osInsecure,
                                 osDataStream);
       std::string e;
       if (!os->Init(e))
          {fprintf(stderr, "%s: %s\n", argv[0], e.c_str()); return 4;}
       osEnabled = true;
#else
       fprintf(stderr, "%s: --os-url requires building with libcurl\n", argv[0]);
       return 2;
#endif
      }

// The file/stdout sink is the fallback when no network sink is configured.
// -o always enables a file in addition to any OpenSearch/forward sink.
//
   bool  fileSink = outFile || (!osEnabled && fwdPort <= 0);
   FILE* out      = stdout;
   if (outFile && !(out = fopen(outFile, "a")))
      {fprintf(stderr, "%s: cannot open '%s': %s\n", argv[0], outFile,
               strerror(errno)); return 4;}

// Optional NDJSON-over-TCP forwarding sink (to a buffering frontend).
//
   XrdMonForward* fwd = fwdPort > 0 ? new XrdMonForward(fwdHost, fwdPort)
                                    : nullptr;
   size_t fwdDrops = 0;
   time_t fwdWarn  = 0;

// Create and bind the UDP socket (dual-stack by default)
//
   int fd = openUDP(port, bindStr);
   if (fd < 0) return 4;

// Batch state for the OpenSearch sink and a flush helper.
//
   std::string batch;
   size_t      batchCount = 0;
   time_t      lastFlush  = time(0);
   auto flush = [&]()
      {
#ifdef XRDMON_HAVE_CURL
       if (osEnabled && batchCount > 0)
          {std::string e;
           if (!os->Bulk(batch, e))
              fprintf(stderr, "xrdmoncollect: bulk post failed: %s\n", e.c_str());
              else if (!e.empty())
                 fprintf(stderr, "xrdmoncollect: %s\n", e.c_str());
           batch.clear(); batchCount = 0;
          }
#endif
       lastFlush = time(0);
      };

// Wire up the sinks. With --bulk the file sink is written in OpenSearch _bulk
// format; the OpenSearch sink batches documents and posts them via _bulk.
//
   XrdMonDecode::DocSink docSink =
      [&](const std::string& d)
         {if (fileSink)
             {if (bulkIdx.empty()) fprintf(out, "%s\n", d.c_str());
                 else fprintf(out, "{\"index\":{\"_index\":\"%s\"}}\n%s\n",
                              bulkIdx.c_str(), d.c_str());
             }
#ifdef XRDMON_HAVE_CURL
          if (osEnabled) {os->Add(batch, d); batchCount++;}
#endif
          if (fwd)
             {std::string e;
              if (!fwd->Send(d, e))
                 {fwdDrops++;
                  // Rate-limit the warning to at most once every 10 seconds.
                  time_t now = time(0);
                  if (now - fwdWarn >= 10)
                     {fwdWarn = now;
                      fprintf(stderr, "xrdmoncollect: forward dropped %zu doc(s): "
                              "%s\n", fwdDrops, e.c_str());
                     }
                 }
             }
         };

   XrdMonDecode::RawSink rawSink;
   if (dump) rawSink = [&](const std::string& r){fprintf(out, "%s\n", r.c_str());};

// When a metrics port is given, aggregate transfers into the registry and
// serve it over HTTP. The decoder-level statistics are exposed too.
//
   XrdMetrics::MetricGroup* reg =
            metricsPort > 0 ? &collectorRegistry().group("") : nullptr;

   XrdMonDecode decoder(docSink, rawSink, dump, traces, gstream, redirects, reg);
   decoder.SetMaxEntries(maxEntries);
   decoder.SetResolveHosts(resolve);
   if (!scitags.empty() && !decoder.LoadScitags(scitags))
      fprintf(stderr, "%s: warning: could not load SciTags registry '%s';"
                      " experiment/activity ids stay numeric\n",
              argv[0], scitags.c_str());

   std::atomic<bool> exporterStop{false};
   std::thread       exporter;
   if (reg)
      {auto& s = decoder.GetStats();
#define OBS(name, help, fld) \
       reg->observeCounter(name, {}, {}, help).add({}, [&]{return (uint64_t)s.fld;})
       OBS("xrootd_collector_packets_total",   "monitor packets received", packets);
       OBS("xrootd_collector_malformed_total", "malformed packets", malformed);
       OBS("xrootd_collector_evicted_total",
           "dictionary/open-file entries evicted by the cap", evicted);
       OBS("xrootd_collector_documents_total", "transfer documents produced", docs);
       OBS("xrootd_collector_orphan_closes_total",
           "closes with no matching open", orphanCls);
       OBS("xrootd_collector_disconnects_total",
           "f-stream session disconnect records", discs);
       OBS("xrootd_collector_trace_records_total", "t-stream records decoded", traces);
       OBS("xrootd_collector_gstream_records_total", "g-stream records decoded", gevents);
       OBS("xrootd_collector_redirect_records_total",
           "r-stream redirect records decoded", redirs);
       OBS("xrootd_collector_frm_records_total",
           "x/p FRM stage/purge records decoded", frmEvents);
       OBS("xrootd_collector_token_records_total",
           "T-stream token records decoded", mapTokn);
       OBS("xrootd_collector_ident_records_total",
           "=-stream server-identity records decoded", mapIdnt);
#undef OBS
       exporter = std::thread(serveMetrics, metricsPort, std::ref(exporterStop));
      }

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

// A 1s receive timeout lets the loop wake to honor the time-based flush even
// when no packets are arriving.
//
   struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
   setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

   char buff[64*1024];
   while(!stopFlag)
        {sockaddr_storage from;
         socklen_t fromLen = sizeof(from);
         ssize_t n = recvfrom(fd, buff, sizeof(buff), 0,
                              (sockaddr*)&from, &fromLen);
         if (n < 0)
            {if (errno == EINTR) continue;
             if (errno == EAGAIN || errno == EWOULDBLOCK)
                {if (batchCount > 0 && time(0)-lastFlush >= flushSecs) flush();
                 continue;
                }
             perror("recvfrom"); break;
            }

         decoder.Process(senderName((sockaddr*)&from, fromLen), buff, (int)n);
         if (fileSink) fflush(out);

         if (batchCount >= flushCount
         || (batchCount > 0 && time(0)-lastFlush >= flushSecs)) flush();
        }

   flush();  // send anything still batched

   if (exporter.joinable()) {exporterStop = true; exporter.join();}

   if (verbose)
      {const XrdMonDecode::Stats& s = decoder.GetStats();
       fprintf(stderr,
         "xrdmoncollect: packets=%llu malformed=%llu records=%llu "
         "mapUser=%llu mapTokn=%llu mapUeac=%llu mapIdnt=%llu "
         "opens=%llu closes=%llu xfrs=%llu discs=%llu docs=%llu "
         "orphanCloses=%llu lost=%llu evicted=%llu "
         "traces=%llu gevents=%llu redirs=%llu frm=%llu unknown=%llu\n",
         (unsigned long long)s.packets, (unsigned long long)s.malformed,
         (unsigned long long)s.records, (unsigned long long)s.mapUser,
         (unsigned long long)s.mapTokn, (unsigned long long)s.mapUeac,
         (unsigned long long)s.mapIdnt,
         (unsigned long long)s.opens, (unsigned long long)s.closes,
         (unsigned long long)s.xfrs, (unsigned long long)s.discs,
         (unsigned long long)s.docs, (unsigned long long)s.orphanCls,
         (unsigned long long)s.lost, (unsigned long long)s.evicted,
         (unsigned long long)s.traces, (unsigned long long)s.gevents,
         (unsigned long long)s.redirs, (unsigned long long)s.frmEvents,
         (unsigned long long)s.unknown);
      }

   if (out != stdout) fclose(out);
   close(fd);
   delete fwd;
#ifdef XRDMON_HAVE_CURL
   delete os;
#endif
   return 0;
}

#ifndef __XRDMONTCPSERVER_HH__
#define __XRDMONTCPSERVER_HH__
/******************************************************************************/
/*                                                                            */
/*                  X r d M o n T c p S e r v e r . h h                       */
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

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "XrdApps/XrdMonCollect/XrdMonPipe.hh"
#include "XrdApps/XrdMonCollect/XrdMonShovelFrame.hh"

//-----------------------------------------------------------------------------
//! The collector's TCP listener for shoveled monitoring packets: it accepts
//! connections from remote xrdmoncollect instances running in shoveler mode,
//! validates the XSHV hello (optionally requiring a shared-secret token),
//! decodes the encapsulated UDP datagrams, and produces them into the same
//! receive pipe as the local UDP receiver — attributed to the datagram's
//! original source, so the decoder cannot tell the transports apart.
//!
//! Thread model: one accept thread plus one thread per connection (the
//! deployment ceiling is one shoveler per cluster node, ~150 connections, so
//! blocking per-connection threads keep the framing parser and batching state
//! strictly private with no event-loop state machine). A hard connection cap
//! bounds the thread count; excess connections are closed on accept.
//-----------------------------------------------------------------------------

class XrdMonTcpServer
{
public:

   //! @param port       TCP port to listen on.
   //! @param bindStr    address to bind, or nullptr for dual-stack any.
   //! @param token      shared secret; empty accepts any hello.
   //! @param pipe       the receive pipe shared with the UDP receiver.
   //! @param flushCount packets per batch before it is submitted.
   //! @param flushSecs  max age of a partial batch before it is submitted.
   XrdMonTcpServer(int port, const char* bindStr, std::string token,
                   XrdMonPipe<XrdMonBatch>& pipe,
                   std::size_t flushCount, long flushSecs)
                  : port(port), bindStr(bindStr ? bindStr : ""),
                    token(std::move(token)), pipe(pipe),
                    flushCount(flushCount ? flushCount : 1),
                    flushSecs(flushSecs > 0 ? flushSecs : 1) {}

   ~XrdMonTcpServer() { Stop(); }

   //! Bind, listen, and start the accept thread. False (with err) on failure.
   bool Start(std::string& err);

   //! Close the listener and all connections and join every thread. Must be
   //! called BEFORE the receive pipe is closed (the connection threads are
   //! producers). Idempotent.
   void Stop();

   // Observable counters for the metrics exporter.
   std::uint64_t Connections()  const { return nConns.load(); }
   std::int64_t  Active()       const { return nActive.load(); }
   std::uint64_t AuthFailures() const { return nAuthFail.load(); }
   std::uint64_t Frames()       const { return nFrames.load(); }
   std::uint64_t Malformed()    const { return nMalformed.load(); }
   std::uint64_t BytesIn()      const { return nBytes.load(); }

private:

   void acceptLoop();
   void serveConn(int fd);

   int         port;
   std::string bindStr;
   std::string token;
   XrdMonPipe<XrdMonBatch>& pipe;
   std::size_t flushCount;
   long        flushSecs;

   int               listenFd = -1;
   std::thread       accepter;
   std::atomic<bool> stopping{false};

   // Live connection threads, keyed by fd so Stop() can shutdown() the
   // sockets to unblock their recv() before joining. A thread whose
   // connection ended moves itself to `done` (joined by the accept thread
   // and Stop, so the thread map does not grow with connection churn).
   std::mutex                  connMtx;
   std::map<int, std::thread>  conns;
   std::deque<std::thread>     done;

   std::atomic<std::uint64_t> nConns{0};
   std::atomic<std::int64_t>  nActive{0};
   std::atomic<std::uint64_t> nAuthFail{0};
   std::atomic<std::uint64_t> nFrames{0};
   std::atomic<std::uint64_t> nMalformed{0};
   std::atomic<std::uint64_t> nBytes{0};
};
#endif

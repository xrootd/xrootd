#ifndef __XRDMONDECODE_HH__
#define __XRDMONDECODE_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d M o n D e c o d e . h h                          */
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

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

//-----------------------------------------------------------------------------
//! XrdMonDecode decodes XRootD detailed-monitoring UDP packets
//! (xrootd.monitor) and correlates the "f" (file-stats) stream against the
//! user dictionary into one document per completed transfer (file close).
//!
//! It is deliberately transport-agnostic: feed it datagrams via Process() and
//! receive finished transfer documents (and, optionally, raw per-record dumps)
//! through callbacks. All multi-byte fields in the wire format are network
//! byte order; see src/XrdXrootd/XrdXrootdMonData.hh for the layouts.
//-----------------------------------------------------------------------------

class XrdMonDecode
{
public:

//! Called with one finished transfer document, serialized as a single JSON
//! object (no trailing newline). The caller frames it for its sink.
using DocSink = std::function<void(const std::string& jsonDoc)>;

//! Optional: called with one JSON object per decoded record (dump mode).
using RawSink = std::function<void(const std::string& jsonRec)>;

//! Counters describing what has been decoded so far.
struct Stats
{
   uint64_t packets   = 0;   // datagrams seen
   uint64_t malformed = 0;   // datagrams rejected as too short / inconsistent
   uint64_t records   = 0;   // individual records decoded
   uint64_t mapUser   = 0;   // 'u' dictionary records
   uint64_t mapPath   = 0;   // 'd' dictionary records
   uint64_t mapInfo   = 0;   // 'i' dictionary records
   uint64_t opens     = 0;   // 'f' open records
   uint64_t closes    = 0;   // 'f' close records
   uint64_t docs      = 0;   // transfer documents emitted
   uint64_t orphanCls = 0;   // closes with no matching open
   uint64_t unknown   = 0;   // packets with an unhandled code
};

//! Process one UDP datagram received from sender `src` (used, together with
//! the server start time in the header, to key per-server state).
//!
//! @return true if the packet was structurally valid, false if malformed.
bool Process(const std::string& src, const char* buff, int blen);

const Stats& GetStats() const {return stats;}

         XrdMonDecode(DocSink docSink, RawSink rawSink = nullptr,
                      bool emitRaw = false)
                     : doc(std::move(docSink)), raw(std::move(rawSink)),
                       dumpRaw(emitRaw) {}
        ~XrdMonDecode() {}

private:

// Identity parsed from a 'u' (MAPUSER) dictionary entry.
//
struct UserInfo
{
   std::string raw;       // full first line of the map info
   std::string user;
   std::string prot;
   std::string host;
};

// An open file awaiting its close, from the 'f' stream open record.
//
struct OpenFile
{
   std::string lfn;
   uint32_t    user = 0;  // user dictid (0 if user monitoring off)
   int64_t     fsz  = 0;  // file size at open
   int32_t     tOpen = 0; // window time of the open packet
   bool        rw   = false;
};

// Per server-incarnation state, keyed by sender + server start time (stod).
//
struct Server
{
   std::unordered_map<uint32_t, UserInfo> users;
   std::unordered_map<uint32_t, OpenFile> files;
   int64_t sID = 0;
};

Server&  ServerFor(const std::string& src, int32_t stod);
void     DecodeMap(unsigned char code, Server& srv,
                   uint32_t dictid, const char* info, int ilen);
void     DecodeFStream(const std::string& src, int32_t stod, Server& srv,
                       const unsigned char* p, int len);
void     EmitClose(const std::string& src, int32_t stod, Server& srv,
                   uint32_t fileID, unsigned char recFlag,
                   const unsigned char* rec, int recSize, int32_t tWin);

std::unordered_map<std::string, Server> servers;
DocSink  doc;
RawSink  raw;
bool     dumpRaw;
Stats    stats;
};
#endif

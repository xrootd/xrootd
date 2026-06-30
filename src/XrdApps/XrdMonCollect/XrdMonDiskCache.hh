#ifndef __XRDMONDISKCACHE_HH__
#define __XRDMONDISKCACHE_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d M o n D i s k C a c h e . h h                      */
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
#include <functional>
#include <string>

//-----------------------------------------------------------------------------
//! On-failure disk cache for serialized OpenSearch `_bulk` bodies. When a POST
//! fails after retries, the body is written to a file in the cache directory and
//! retried later (oldest first), surviving process restarts. Files are named
//! `<epoch_ms>-<seq>.ndjson` (zero-padded so lexical order is chronological) and
//! written via a `.tmp` partial + atomic rename, so a crash never leaves a
//! half-written body to be replayed.
//!
//! Used exclusively by the collector's output thread, so the pending list needs
//! no locking; only the metric accessors are atomic, for the HTTP exporter to
//! read concurrently.
//-----------------------------------------------------------------------------

class XrdMonDiskCache
{
public:

   explicit XrdMonDiskCache(std::string dir) : dir(std::move(dir)) {}

   //! Create the directory if needed and seed the pending list from files left
   //! by a previous run (oldest first), removing any stale `.tmp` partials.
   //! Returns false (with err) on a directory error.
   bool Init(std::string& err);

   //! Persist one body. Returns false (with err) on an I/O error.
   bool Store(const std::string& body, std::string& err);

   //! Whether any cached bodies are awaiting replay.
   bool Empty() const { return pending.empty(); }

   //! Replay the oldest cached body through `cb`. Returns 1 if a body was
   //! replayed and removed (cb returned true, or the file was unreadable and so
   //! dropped — see err), 0 if nothing is pending, and -1 if cb reported the
   //! sink still unavailable (the file is kept for a later attempt).
   int ReplayOldest(const std::function<bool(const std::string&)>& cb,
                    std::string& err);

   std::size_t   Files()    const { return files.load(); }
   std::uint64_t Bytes()    const { return bytes.load(); }
   std::uint64_t Stored()   const { return stored.load(); }
   std::uint64_t Replayed() const { return replayed.load(); }

private:

   std::string path(const std::string& name) const { return dir + "/" + name; }

   std::string                dir;
   std::deque<std::string>    pending;       // file names, oldest first
   std::uint64_t              seq = 0;        // disambiguates same-millisecond
   std::atomic<std::size_t>   files{0};
   std::atomic<std::uint64_t> bytes{0};
   std::atomic<std::uint64_t> stored{0};
   std::atomic<std::uint64_t> replayed{0};
};

#endif

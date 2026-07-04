#ifndef __XRDMONPIPE_HH__
#define __XRDMONPIPE_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d M o n P i p e . h h                            */
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

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

//-----------------------------------------------------------------------------
//! A bounded producer/consumer hand-off with buffer recycling, used to connect
//! the xrdmoncollect pipeline stages (receiver -> serializer -> output). Every
//! operation takes the internal mutex, so multiple producers may acquire() and
//! submit() concurrently (the UDP receiver and the TCP connection threads);
//! the consumer side is single-threaded. The producer `acquire()`s a recycled
//! (or freshly created) buffer, fills it, and `submit()`s it; the consumer
//! `take()`s it, drains it, and `recycle()`s it back. At most `capacity`
//! buffers ever exist, so a slow consumer applies backpressure: `acquire()`
//! blocks until a buffer is free rather than letting the producer allocate
//! without bound.
//!
//! Recycling avoids per-hand-off allocation churn (the same buffers cycle round
//! the loop). `close()` unblocks both ends for shutdown: after it, `acquire()`
//! returns false and `take()` returns false once the ready queue is drained.
//-----------------------------------------------------------------------------

template<class T>
class XrdMonPipe
{
public:

   explicit XrdMonPipe(std::size_t capacity) : cap(capacity ? capacity : 1) {}

   //! Producer: obtain an empty buffer to fill (a recycled one if available,
   //! else a new one until `capacity` exist, otherwise block until one is
   //! recycled). Returns false if the pipe was closed.
   bool acquire(T& out)
   {
      std::unique_lock<std::mutex> lk(mtx);
      space.wait(lk, [&]{ return closed || !freeQ.empty() || live < cap; });
      if (closed) return false;
      if (!freeQ.empty()) {out = std::move(freeQ.front()); freeQ.pop_front();}
      else                {out = T{}; ++live;}
      return true;
   }

   //! Producer: hand a filled buffer to the consumer.
   void submit(T&& v)
   {
      {std::lock_guard<std::mutex> lk(mtx); readyQ.push_back(std::move(v));}
      data.notify_one();
   }

   //! Consumer: take the next filled buffer. Returns false once the pipe is
   //! closed and fully drained.
   bool take(T& out)
   {
      std::unique_lock<std::mutex> lk(mtx);
      data.wait(lk, [&]{ return closed || !readyQ.empty(); });
      if (readyQ.empty()) return false;          // closed and drained
      out = std::move(readyQ.front()); readyQ.pop_front();
      return true;
   }

   //! Consumer: like take(), but wait at most `ms` milliseconds. Returns true
   //! with `out` set if a buffer was taken; false on timeout or once the pipe is
   //! closed and drained (use closedDrained() to tell those apart). Lets a
   //! consumer wake periodically to do other work (e.g. drain a backlog).
   bool takeFor(T& out, int ms)
   {
      std::unique_lock<std::mutex> lk(mtx);
      data.wait_for(lk, std::chrono::milliseconds(ms),
                    [&]{ return closed || !readyQ.empty(); });
      if (readyQ.empty()) return false;          // timeout, or closed & drained
      out = std::move(readyQ.front()); readyQ.pop_front();
      return true;
   }

   //! True once the pipe is closed and no filled buffers remain.
   bool closedDrained()
   {
      std::lock_guard<std::mutex> lk(mtx);
      return closed && readyQ.empty();
   }

   //! Consumer: return an emptied buffer for reuse (the caller should clear it
   //! first so the producer receives it empty).
   void recycle(T&& v)
   {
      {std::lock_guard<std::mutex> lk(mtx); freeQ.push_back(std::move(v));}
      space.notify_one();
   }

   //! Unblock both ends for shutdown.
   void close()
   {
      {std::lock_guard<std::mutex> lk(mtx); closed = true;}
      data.notify_all();
      space.notify_all();
   }

   //! Number of filled buffers waiting for the consumer (for a depth gauge).
   std::size_t readyDepth()
   {
      std::lock_guard<std::mutex> lk(mtx);
      return readyQ.size();
   }

private:
   std::mutex              mtx;
   std::condition_variable data;   // signaled when a buffer becomes ready
   std::condition_variable space;  // signaled when a buffer becomes free
   std::deque<T>           readyQ; // filled, awaiting the consumer
   std::deque<T>           freeQ;  // recycled, awaiting the producer
   std::size_t             cap;    // max buffers in existence
   std::size_t             live = 0; // buffers created so far (<= cap)
   bool                    closed = false;
};

#endif

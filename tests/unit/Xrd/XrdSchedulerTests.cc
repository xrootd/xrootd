/******************************************************************************/
/*                                                                            */
/*                 X r d S c h e d u l e r T e s t s . c c                    */
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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <thread>

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"

namespace
{

// A timed job that re-arms itself from DoIt(), as the g-stream auto-flush
// does. Asking for time(0) makes the timer pop it again at once, so the
// scheduler goes through "pop, then wait on a queue that looks empty" as fast
// as it can, and a signal lost in that window shows up within seconds.
class RearmJob : public XrdJob
{
public:
   RearmJob(XrdScheduler &s) : XrdJob(".rearm"), sched(s) {}

   void DoIt() override
   {
      runs.fetch_add(1, std::memory_order_relaxed);
      if (!stop.load()) sched.Schedule(this, time(0));
   }

   std::atomic<long> runs{0};
   std::atomic<bool> stop{false};

private:
   XrdScheduler &sched;
};

double Now()
{
   using namespace std::chrono;
   return duration<double>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

// A lost timer wakeup leaves the timer thread asleep until its wait expires,
// and no timed job runs meanwhile. With nothing else queued here but the
// idle-worker check, that wait is long, so a gap of a few seconds is the bug.
TEST(XrdScheduler, TimedJobsKeepRunning)
{
   const double duration = 10, maxGap = 3;

   // Neither object is destroyed. A scheduler whose timer thread is stuck
   // cannot be shut down, and a failing run must still end the test.
   XrdScheduler *sched = new XrdScheduler(3, 128, 780);
   sched->Start();
   RearmJob *job = new RearmJob(*sched);
   sched->Schedule(job, time(0));

   double t0 = Now(), tLast = t0, worstGap = 0;
   long last = 0;
   while (Now() - t0 < duration)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      long r = job->runs.load();
      double t = Now();
      if (r != last) { last = r; tLast = t; }
      if (t - tLast > worstGap) worstGap = t - tLast;
      if (worstGap > maxGap) break;
   }
   job->stop = true;

   EXPECT_LE(worstGap, maxGap) << "no timed job ran for " << worstGap
                               << " s, after " << last << " runs";
   EXPECT_GT(last, 0);
}

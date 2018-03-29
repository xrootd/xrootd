
#include "XrdThrottleManager.hh"

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysTimer.hh"

#include "XrdOuc/XrdOucEnv.hh"

#define XRD_TRACE m_trace->
#include "XrdThrottle/XrdThrottleTrace.hh"

const char *
XrdThrottleManager::TraceID = "ThrottleManager";

const
int XrdThrottleManager::m_max_users = 1024;

#if defined(__linux__)
int clock_id;
int XrdThrottleTimer::clock_id = clock_getcpuclockid(0, &clock_id) != ENOENT ? CLOCK_THREAD_CPUTIME_ID : CLOCK_MONOTONIC;
#else
int XrdThrottleTimer::clock_id = 0;
#endif

XrdThrottleManager::XrdThrottleManager(XrdSysError *lP, XrdOucTrace *tP) :
   m_trace(tP),
   m_log(lP),
   m_interval_length_seconds(1.0),
   m_bytes_per_second(-1),
   m_ops_per_second(-1),
   m_concurrency_limit(-1),
   m_last_round_allocation(100*1024),
   m_io_counter(0),
   m_loadshed_host(""),
   m_loadshed_port(0),
   m_loadshed_frequency(0),
   m_loadshed_limit_hit(0)
{
   m_stable_io_wait.tv_sec = 0;
   m_stable_io_wait.tv_nsec = 0;
}

void
XrdThrottleManager::Init()
{
   TRACE(DEBUG, "Initializing the throttle manager.");
   // Initialize all our shares to zero.
   m_primary_bytes_shares.reserve(m_max_users);
   m_secondary_bytes_shares.reserve(m_max_users);
   m_primary_ops_shares.reserve(m_max_users);
   m_secondary_ops_shares.reserve(m_max_users);
   // Allocate each user 100KB and 10 ops to bootstrap;
   for (int i=0; i<m_max_users; i++)
   {
      m_primary_bytes_shares[i] = m_last_round_allocation;
      m_secondary_bytes_shares[i] = 0;
      m_primary_ops_shares[i] = 10;
      m_secondary_ops_shares[i] = 0;
   }

   m_io_wait.tv_sec = 0;
   m_io_wait.tv_nsec = 0;

   int rc;
   pthread_t tid;
   if ((rc = XrdSysThread::Run(&tid, XrdThrottleManager::RecomputeBootstrap, static_cast<void *>(this), 0, "Buffer Manager throttle")))
      m_log->Emsg("ThrottleManager", rc, "create throttle thread");

}

/*
 * Take as many shares as possible to fulfill the request; update
 * request with current remaining value, or zero if satisfied.
 */
inline void
XrdThrottleManager::GetShares(int &shares, int &request)
{
   int remaining;
   AtomicFSub(remaining, shares, request);
   if (remaining > 0)
   {
      request -= (remaining < request) ? remaining : request;
   }
}

/*
 * Iterate through all of the secondary shares, attempting 
 * to steal enough to fulfill the request.
 */
void
XrdThrottleManager::StealShares(int uid, int &reqsize, int &reqops)
{
   if (!reqsize && !reqops) return;
   TRACE(BANDWIDTH, "Stealing shares to fill request of " << reqsize << " bytes");
   TRACE(IOPS, "Stealing shares to fill request of " << reqops << " ops.");

   for (int i=uid+1; i % m_max_users == uid; i++)
   {
      if (reqsize) GetShares(m_secondary_bytes_shares[i % m_max_users], reqsize);
      if (reqops)  GetShares(m_secondary_ops_shares[  i % m_max_users], reqops);
   }

   TRACE(BANDWIDTH, "After stealing shares, " << reqsize << " of request bytes remain.");
   TRACE(IOPS, "After stealing shares, " << reqops << " of request ops remain.");
}

/*
 * Apply the throttle.  If there are no limits set, returns immediately.  Otherwise,
 * this applies the limits as best possible, stalling the thread if necessary.
 */
void
XrdThrottleManager::Apply(int reqsize, int reqops, int uid)
{
   if (m_bytes_per_second < 0)
      reqsize = 0;
   if (m_ops_per_second < 0)
      reqops = 0;
   while (reqsize || reqops)
   {
      // Subtract the requested out of the shares
      AtomicBeg(m_compute_var);
      GetShares(m_primary_bytes_shares[uid], reqsize);
      if (reqsize)
      {
         TRACE(BANDWIDTH, "Using secondary shares; request has " << reqsize << " bytes left.");
         GetShares(m_secondary_bytes_shares[uid], reqsize);
         TRACE(BANDWIDTH, "Finished with secondary shares; request has " << reqsize << " bytes left.");
      }
      else
      {
         TRACE(BANDWIDTH, "Filled byte shares out of primary; " << m_primary_bytes_shares[uid] << " left.");
      }
      GetShares(m_primary_ops_shares[uid], reqops);
      if (reqops)
      {
         GetShares(m_secondary_ops_shares[uid], reqops);
      }
      StealShares(uid, reqsize, reqops);
      AtomicEnd(m_compute_var);

      if (reqsize || reqops)
      {
         if (reqsize) TRACE(BANDWIDTH, "Sleeping to wait for throttle fairshare.");
         if (reqops) TRACE(IOPS, "Sleeping to wait for throttle fairshare.");
         m_compute_var.Wait();
         AtomicBeg(m_compute_var);
         AtomicInc(m_loadshed_limit_hit);
         AtomicEnd(m_compute_var);
      }
   }

}

void *
XrdThrottleManager::RecomputeBootstrap(void *instance)
{
   XrdThrottleManager * manager = static_cast<XrdThrottleManager*>(instance);
   manager->Recompute();
   return NULL;
}

void
XrdThrottleManager::Recompute()
{
   while (1)
   {
      TRACE(DEBUG, "Recomputing fairshares for throttle.");
      RecomputeInternal();
      TRACE(DEBUG, "Finished recomputing fairshares for throttle; sleeping for " << m_interval_length_seconds << " seconds.");
      XrdSysTimer::Wait(static_cast<int>(1000*m_interval_length_seconds));
   }
}

/*
 * The heart of the manager approach.
 *
 * This routine periodically recomputes the shares of each current user.
 * Each user has a "primary" and a "secondary" share.  At the end of the
 * each time interval, the remaining primary share is moved to secondary.
 * A user can utilize both shares; if both are gone, they must block until
 * the next recompute interval.
 *
 * The secondary share can be "stolen" by any other user; so, if a user
 * is idle or under-utilizing, their share can be used by someone else.
 * However, they can never be completely starved, as no one can steal
 * primary share.
 *
 * In this way, we violate the throttle for an interval, but never starve.
 *
 */
void
XrdThrottleManager::RecomputeInternal()
{
   // Compute total shares for this interval; 
   float intervals_per_second = 1.0/m_interval_length_seconds;
   float total_bytes_shares = m_bytes_per_second / intervals_per_second;
   float total_ops_shares   = m_ops_per_second / intervals_per_second;

   // Compute the number of active users; a user is active if they used
   // any primary share during the last interval;
   AtomicBeg(m_compute_var);
   float active_users = 0;
   long bytes_used = 0;
   for (int i=0; i<m_max_users; i++)
   {
      int primary = AtomicFAZ(m_primary_bytes_shares[i]);
      if (primary != m_last_round_allocation)
      {
         active_users++;
         if (primary >= 0)
            m_secondary_bytes_shares[i] = primary;
         primary = AtomicFAZ(m_primary_ops_shares[i]);
         if (primary >= 0)
             m_secondary_ops_shares[i] = primary;
         bytes_used += (primary < 0) ? m_last_round_allocation : (m_last_round_allocation-primary);
      }
   }

   if (active_users == 0)
   {
      active_users++;
   }

   // Note we allocate the same number of shares to *all* users, not
   // just the active ones.  If a new user becomes active in the next
   // interval, we'll go over our bandwidth budget just a bit.
   m_last_round_allocation = static_cast<int>(total_bytes_shares / active_users);
   int ops_shares = static_cast<int>(total_ops_shares / active_users);
   TRACE(BANDWIDTH, "Round byte allocation " << m_last_round_allocation << " ; last round used " << bytes_used << ".");
   TRACE(IOPS, "Round ops allocation " << ops_shares);
   for (int i=0; i<m_max_users; i++)
   {
      m_primary_bytes_shares[i] = m_last_round_allocation;
      m_primary_ops_shares[i] = ops_shares;
   }

   // Reset the loadshed limit counter.
   int limit_hit = AtomicFAZ(m_loadshed_limit_hit);
   TRACE(DEBUG, "Throttle limit hit " << limit_hit << " times during last interval.");

   AtomicEnd(m_compute_var);

   // Update the IO counters
   m_compute_var.Lock();
   m_stable_io_counter = AtomicGet(m_io_counter);
   time_t secs; AtomicFZAP(secs, m_io_wait.tv_sec);
   long nsecs; AtomicFZAP(nsecs, m_io_wait.tv_nsec);
   m_stable_io_wait.tv_sec += static_cast<long>(secs * intervals_per_second);
   m_stable_io_wait.tv_nsec += static_cast<long>(nsecs * intervals_per_second);
   while (m_stable_io_wait.tv_nsec > 1000000000)
   {
      m_stable_io_wait.tv_nsec -= 1000000000;
      m_stable_io_wait.tv_nsec --;
   }
   m_compute_var.UnLock();
   TRACE(IOLOAD, "Current IO counter is " << m_stable_io_counter << "; total IO wait time is " << (m_stable_io_wait.tv_sec*1000+m_stable_io_wait.tv_nsec/1000000) << "ms.");
   m_compute_var.Broadcast();
}

/*
 * Do a simple hash across the username.
 */
int
XrdThrottleManager::GetUid(const char *username)
{
   const char *cur = username;
   int hval = 0;
   while (cur && *cur && *cur != '@' && *cur != '.')
   {
      hval += *cur;
      hval %= m_max_users;
      cur++;
   }
   //cerr << "Calculated UID " << hval << " for " << username << endl;
   return hval;
}

/*
 * Create an IO timer object; increment the number of outstanding IOs.
 */
XrdThrottleTimer
XrdThrottleManager::StartIOTimer()
{
   AtomicBeg(m_compute_var);
   int cur_counter = AtomicInc(m_io_counter);
   AtomicEnd(m_compute_var);
   while (m_concurrency_limit >= 0 && cur_counter > m_concurrency_limit)
   {
      AtomicBeg(m_compute_var);
      AtomicInc(m_loadshed_limit_hit);
      AtomicDec(m_io_counter);
      AtomicEnd(m_compute_var);
      m_compute_var.Wait();
      AtomicBeg(m_compute_var);
      cur_counter = AtomicInc(m_io_counter);
      AtomicEnd(m_compute_var);
   }
   return XrdThrottleTimer(*this);
}

/*
 * Finish recording an IO timer.
 */
void
XrdThrottleManager::StopIOTimer(struct timespec timer)
{
   AtomicBeg(m_compute_var);
   AtomicDec(m_io_counter);
   AtomicAdd(m_io_wait.tv_sec, timer.tv_sec);
   // Note this may result in tv_nsec > 1e9
   AtomicAdd(m_io_wait.tv_nsec, timer.tv_nsec);
   AtomicEnd(m_compute_var);
}

/*
 * Check the counters to see if we have hit any throttle limits in the
 * current time period.  If so, shed the client randomly.
 *
 * If the client has already been load-shedded once and reconnected to this
 * server, then do not load-shed it again.
 */
bool
XrdThrottleManager::CheckLoadShed(const std::string &opaque)
{
   if (m_loadshed_port == 0)
   {
      return false;
   }
   if (AtomicGet(m_loadshed_limit_hit) == 0)
   {
      return false;
   }
   if (static_cast<unsigned>(rand()) % 100 > m_loadshed_frequency)
   {
      return false;
   }
   if (opaque.empty())
   {
      return false;
   }
   return true;
}

void
XrdThrottleManager::PrepLoadShed(const char * opaque, std::string &lsOpaque)
{
   if (m_loadshed_port == 0)
   {
      return;
   }
   if (opaque && opaque[0])
   {
      XrdOucEnv env(opaque);
      // Do not load shed client if it has already been done once.
      if (env.Get("throttle.shed") != 0)
      {
         return;
      }
      lsOpaque = opaque;
      lsOpaque += "&throttle.shed=1";
   }
   else
   {
      lsOpaque = "throttle.shed=1";
   }
}

void
XrdThrottleManager::PerformLoadShed(const std::string &opaque, std::string &host, unsigned &port)
{
   host = m_loadshed_host;
   host += "?";
   host += opaque;
   port = m_loadshed_port;
}

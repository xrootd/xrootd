
#include "XrdThrottleManager.hh"

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysTimer.hh"

#define XRD_TRACE m_trace->
#include "Xrd/XrdTrace.hh"

const char *
XrdThrottleManager::TraceID = "ThrottleManager";

const
int XrdThrottleManager::m_max_users = 1024;

XrdThrottleManager::XrdThrottleManager(XrdSysError *lP, XrdOucTrace *tP) :
   m_trace(tP),
   m_log(lP),
   m_interval_length_seconds(1.0),
   m_bytes_per_second(-1),
   m_ops_per_second(-1),
   m_last_round_allocation(100*1024)
{
}

void
XrdThrottleManager::Init()
{
   TRACE(THROTTLE, "Initializing the throttle manager.");
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
   int remaining = AtomicSub(shares, request);
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
   TRACE(THROTTLE, "Stealing shares to fill request of " << reqsize << " bytes, " << reqops << " ops.");

   for (int i=uid+1; i % m_max_users == uid; i++)
   {
      if (reqsize) GetShares(m_secondary_bytes_shares[i % m_max_users], reqsize);
      if (reqops)  GetShares(m_secondary_ops_shares[  i % m_max_users], reqops);
   }
   TRACE(THROTTLE, "After stealing shares, " << reqsize << " of request bytes remain.");
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
         TRACE(THROTTLE, "Using secondary shares; request has " << reqsize << " bytes left.");
         GetShares(m_secondary_bytes_shares[uid], reqsize);
         TRACE(THROTTLE, "Finished with secondary shares; request has " << reqsize << " bytes left.");
      }
      else
      {
         TRACE(THROTTLE, "Filled byte shares out of primary; " << m_primary_bytes_shares[uid] << " left.");
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
         TRACE(THROTTLE, "Sleeping to wait for throttle fairshare.");
         m_compute_var.Wait();
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
      TRACE(THROTTLE, "Recomputing fairshares for throttle.");
      RecomputeInternal();
      TRACE(THROTTLE, "Finished recomputing fairshares for throttle; sleeping for " << m_interval_length_seconds << " seconds.");
      XrdSysTimer::Wait(1000*m_interval_length_seconds);
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
   m_last_round_allocation = (total_bytes_shares / active_users);
   int ops_shares = (total_ops_shares / active_users);
   TRACE(THROTTLE, "Round byte allocation " << m_last_round_allocation << "; ops allocation " << ops_shares << " ; last round used " << bytes_used << ".");
   for (int i=0; i<m_max_users; i++)
   {
      m_primary_bytes_shares[i] = m_last_round_allocation;
      m_primary_ops_shares[i] = ops_shares;
   }
   AtomicEnd(m_compute_var);
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
   while (*cur && *cur != '@' && *cur != '.')
   {
      hval += *cur;
      hval %= m_max_users;
      cur++;
   }
   //cerr << "Calculated UID " << hval << " for " << username << endl;
   return hval;
}


#include "XrdThrottleManager.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdThrottle/XrdThrottleConfig.hh"
#include "XrdXrootd/XrdXrootdGStream.hh"

#define XRD_TRACE m_trace->
#include "XrdThrottle/XrdThrottleTrace.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <sstream>

#if defined(__linux__)

#include <sched.h>
unsigned XrdThrottleManager::GetTimerListHash() {
    int cpu = sched_getcpu();
    if (cpu < 0) {
        return 0;
    }
    return cpu % m_timer_list_size;
}

#else

unsigned XrdThrottleManager::GetTimerListHash() {
    return 0;
}

#endif

const char *
XrdThrottleManager::TraceID = "ThrottleManager";

XrdThrottleManager::XrdThrottleManager(XrdSysError *lP, XrdOucTrace *tP) :
   m_trace(tP),
   m_log(lP),
   m_interval_length_seconds(1.0),
   m_bytes_per_second(-1),
   m_ops_per_second(-1),
   m_concurrency_limit(-1),
   m_last_round_allocation(100*1024),
   m_loadshed_host(""),
   m_loadshed_port(0),
   m_loadshed_frequency(0)
{
}

void
XrdThrottleManager::FromConfig(XrdThrottle::Configuration &config)
{

    auto max_open = config.GetMaxOpen();
    if (max_open != -1) SetMaxOpen(max_open);
    auto max_conn = config.GetMaxConn();
    if (max_conn != -1) SetMaxConns(max_conn);
    auto max_wait = config.GetMaxWait();
    if (max_wait != -1) SetMaxWait(max_wait);

    SetThrottles(config.GetThrottleDataRate(),
       config.GetThrottleIOPSRate(),
       config.GetThrottleConcurrency(),
       static_cast<float>(config.GetThrottleRecomputeIntervalMS())/1000.0);

    m_trace->What = config.GetTraceLevels();

    auto loadshed_host = config.GetLoadshedHost();
    auto loadshed_port = config.GetLoadshedPort();
    auto loadshed_freq = config.GetLoadshedFreq();
    if (!loadshed_host.empty() && loadshed_port > 0 && loadshed_freq > 0)
    {
       // Loadshed specified, so set it.
       SetLoadShed(loadshed_host, loadshed_port, loadshed_freq);
    }
}

void
XrdThrottleManager::Init()
{
   TRACE(DEBUG, "Initializing the throttle manager.");
   // Initialize all our shares to zero.
   m_primary_bytes_shares.resize(m_max_users);
   m_secondary_bytes_shares.resize(m_max_users);
   m_primary_ops_shares.resize(m_max_users);
   m_secondary_ops_shares.resize(m_max_users);
   for (auto & waiter : m_waiter_info) {
      waiter.m_manager = this;
   }

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

std::tuple<std::string, uint16_t>
XrdThrottleManager::GetUserInfo(const XrdSecEntity *client) {
    // client can be null, if so, return nobody
    if (!client) {
        return std::make_tuple("nobody", GetUid("nobody"));
    }

    // Try various potential "names" associated with the request, from the most
    // specific to most generic.
    std::string user;

    if (client->eaAPI && client->eaAPI->Get("token.subject", user)) {
        if (client->vorg) user = std::string(client->vorg) + ":" + user;
    } else if (client->eaAPI) {
        std::string request_name;
        if (client->eaAPI->Get("request.name", request_name) && !request_name.empty()) user = request_name;
    }
    if (user.empty()) {user = client->name ? client->name : "nobody";}
    uint16_t uid = GetUid(user.c_str());
    return std::make_tuple(user, uid);
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
 * Increment the number of files held open by a given entity.  Returns false
 * if the user is at the maximum; in this case, the internal counter is not
 * incremented.
 */
bool
XrdThrottleManager::OpenFile(const std::string &entity, std::string &error_message)
{
    if (m_max_open == 0 && m_max_conns == 0) return true;

    const std::lock_guard<std::mutex> lock(m_file_mutex);
    auto iter = m_file_counters.find(entity);
    unsigned long cur_open_files = 0, cur_open_conns;
    if (m_max_open) {
        if (iter == m_file_counters.end()) {
            m_file_counters[entity] = 1;
            TRACE(FILES, "User " << entity << " has opened their first file");
            cur_open_files = 1;
        } else if (iter->second < m_max_open) {
            iter->second++;
            cur_open_files = iter->second;
        } else {
            std::stringstream ss;
            ss <<  "User " << entity << " has hit the limit of " << m_max_open << " open files";
            TRACE(FILES, ss.str());
            error_message = ss.str();
            return false;
        }
    }

    if (m_max_conns) {
        auto pid = XrdSysThread::Num();
        auto conn_iter = m_active_conns.find(entity);
        auto conn_count_iter = m_conn_counters.find(entity);
        if ((conn_count_iter != m_conn_counters.end()) && (conn_count_iter->second == m_max_conns) &&
            (conn_iter == m_active_conns.end() || ((*(conn_iter->second))[pid] == 0)))
        {
            // note: we are rolling back the increment in open files
            if (m_max_open) iter->second--;
            std::stringstream ss;
            ss << "User " << entity << " has hit the limit of " << m_max_conns <<
                " open connections";
            TRACE(CONNS, ss.str());
            error_message = ss.str();
            return false;
        }
        if (conn_iter == m_active_conns.end()) {
            std::unique_ptr<std::unordered_map<pid_t, unsigned long>> conn_map(
                new std::unordered_map<pid_t, unsigned long>());
            (*conn_map)[pid] = 1;
            m_active_conns[entity] = std::move(conn_map);
            if (conn_count_iter == m_conn_counters.end()) {
                m_conn_counters[entity] = 1;
                cur_open_conns = 1;
            } else {
                m_conn_counters[entity] ++;
                cur_open_conns = m_conn_counters[entity];
            }
        } else {
            auto pid_iter = conn_iter->second->find(pid);
            if (pid_iter == conn_iter->second->end() || pid_iter->second == 0) {
                (*(conn_iter->second))[pid] = 1;
                conn_count_iter->second++;
                cur_open_conns = conn_count_iter->second;
            } else {
                (*(conn_iter->second))[pid] ++;
                cur_open_conns = conn_count_iter->second;
           }
        }
        TRACE(CONNS, "User " << entity << " has " << cur_open_conns << " open connections");
    }
    if (m_max_open) TRACE(FILES, "User " << entity << " has " << cur_open_files << " open files");
    return true;
}


/*
 * Decrement the number of files held open by a given entity.
 *
 * Returns false if the value would have fallen below zero or
 * if the entity isn't tracked.
 */
bool
XrdThrottleManager::CloseFile(const std::string &entity)
{
    if (m_max_open == 0 && m_max_conns == 0) return true;

    bool result = true;
    const std::lock_guard<std::mutex> lock(m_file_mutex);
    if (m_max_open) {
        auto iter = m_file_counters.find(entity);
        if (iter == m_file_counters.end()) {
            TRACE(FILES, "WARNING: User " << entity << " closed a file but throttle plugin never saw an open file");
            result = false;
        } else if (iter->second == 0) {
            TRACE(FILES, "WARNING: User " << entity << " closed a file but throttle plugin thinks all files were already closed");
            result = false;
        } else {
            iter->second--;
        }
        if (result) TRACE(FILES, "User " << entity << " closed a file; " << iter->second <<
                                 " remain open");
    }

    if (m_max_conns) {
        auto pid = XrdSysThread::Num();
        auto conn_iter = m_active_conns.find(entity);
        auto conn_count_iter = m_conn_counters.find(entity);
        if (conn_iter == m_active_conns.end() || !(conn_iter->second)) {
            TRACE(CONNS, "WARNING: User " << entity << " closed a file on a connection we are not"
                " tracking");
            return false;
        }
        auto pid_iter = conn_iter->second->find(pid);
        if (pid_iter == conn_iter->second->end()) {
            TRACE(CONNS, "WARNING: User " << entity << " closed a file on a connection we are not"
                " tracking");
            return false;
        }
        if (pid_iter->second == 0) {
            TRACE(CONNS, "WARNING: User " << entity << " closed a file on connection the throttle"
                " plugin thinks was idle");
        } else {
            pid_iter->second--;
        }
        if (conn_count_iter == m_conn_counters.end()) {
            TRACE(CONNS, "WARNING: User " << entity << " closed a file but the throttle plugin never"
                " observed an open file");
        } else if (pid_iter->second == 0) {
            if (conn_count_iter->second == 0) {
                TRACE(CONNS, "WARNING: User " << entity << " had a connection go idle but the "
                    " throttle plugin already thought all connections were idle");
            } else {
                conn_count_iter->second--;
                TRACE(CONNS, "User " << entity << " had connection on thread " << pid << " go idle; "
                    << conn_count_iter->second << " active connections remain");
            }
        }
    }

    return result;
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
         m_loadshed_limit_hit++;
      }
   }

}

void
XrdThrottleManager::UserIOAccounting()
{
    std::chrono::steady_clock::duration::rep total_active_time = 0;
    for (size_t idx = 0; idx < m_timer_list.size(); idx++) {
        auto &timerList = m_timer_list[idx];
        std::unique_lock<std::mutex> lock(timerList.m_mutex);
        auto timer = timerList.m_first;
        while (timer) {
            auto next = timer->m_next;
            auto uid = timer->m_owner;
            auto &waiter = m_waiter_info[uid];
            auto recent_duration = timer->Reset();
            waiter.m_io_time += recent_duration.count();

            total_active_time += recent_duration.count();
            timer = next;
        }
    }
    m_io_active_time += total_active_time;
}

void
XrdThrottleManager::ComputeWaiterOrder()
{
    // Update the IO time for long-running I/O operations.  This prevents,
    // for example, a 2-minute I/O operation from causing a spike in
    // concurrency because it's otherwise only reported at the end.
    UserIOAccounting();

    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - m_last_waiter_recompute_time;
    m_last_waiter_recompute_time = now;
    std::chrono::duration<double> elapsed_secs = elapsed;
    // Alpha is the decay factor for the exponential moving average.  One window is 10 seconds,
    // so every 10 seconds we decay the prior average by 1/e (that is, the weight is 64% of the
    // total).  This means the contribution of I/O load from a minute ago is 0.2% of the total.

    // The moving average will be used to determine how close the user is to their "fair share"
    // of the concurrency limit among the users that are waiting.
    auto alpha = 1 - std::exp(-1 * elapsed_secs.count() / 10.0);

    std::vector<double> share;
    share.resize(m_max_users);
    size_t users_with_waiters = 0;
    // For each user, compute their current concurrency and determine how many waiting users
    // total there are.
    for (int i = 0; i < m_max_users; i++)
    {
        auto &waiter = m_waiter_info[i];
        auto io_duration_rep = waiter.m_io_time.exchange(std::chrono::steady_clock::duration(0).count());
        std::chrono::steady_clock::duration io_duration = std::chrono::steady_clock::duration(io_duration_rep);
        std::chrono::duration<double> io_duration_secs = io_duration;
        auto prev_concurrency = io_duration_secs.count() / elapsed_secs.count();
        float new_concurrency = waiter.m_concurrency;

        new_concurrency = (1 - alpha) * new_concurrency + alpha * prev_concurrency;
        waiter.m_concurrency = new_concurrency;
        if (new_concurrency > 0) {
            TRACE(DEBUG, "User " << i << " has concurrency of " << new_concurrency);
        }
        unsigned waiting;
        {
            std::lock_guard<std::mutex> lock(waiter.m_mutex);
            waiting = waiter.m_waiting;
        }
        if (waiting > 0)
        {
            share[i] = new_concurrency;
            TRACE(DEBUG, "User " << i << " has concurrency of " << share[i] << " and is waiting for " << waiting);
            // Handle the division-by-zero case; if we have no history of usage whatsoever, we should pretend we
            // have at least some minimal load
            if (share[i] == 0) {
                share[i] = 0.1;
            }
            users_with_waiters++;
        }
        else
        {
            share[i] = 0;
        }
    }
    auto fair_share = static_cast<double>(m_concurrency_limit) / static_cast<double>(users_with_waiters);
    std::vector<uint16_t> waiter_order;
    waiter_order.resize(m_max_users);

    // Calculate the share for each user.  We assume the user should get a share proportional to how
    // far above or below the fair share they are.  So, a user with concurrency of 20 when the fairshare
    // is 10 will get 0.5 shares; a user with concurrency of 5 when the fairshare is 10 will get 2.0 shares.
    double shares_sum = 0;
    for (int idx = 0; idx < m_max_users; idx++)
    {
        if (share[idx]) {
            shares_sum += fair_share / share[idx];
        }
    }

    // We must quantize the overall shares into an array of 1024 elements.  We do this by
    // scaling up (or down) based on the total number of shares computed above.  Note this
    // quantization can lead to an over-provisioned user being assigned zero shares; thus,
    // we scale based on (1024-#users) so we can give one extra share to each user.
    auto scale_factor = (static_cast<double>(m_max_users) - static_cast<double>(users_with_waiters)) / shares_sum;
    size_t offset = 0;
    for (int uid = 0; uid < m_max_users; uid++) {
        if (share[uid] > 0) {
            auto shares = static_cast<unsigned>(scale_factor * fair_share / share[uid]) + 1;
            TRACE(DEBUG, "User " << uid << " has " << shares << " shares");
            for (unsigned idx = 0; idx < shares; idx++)
            {
                waiter_order[offset % m_max_users] = uid;
                offset++;
            }
        }
    }
    if (offset < m_max_users) {
        for (size_t idx = offset; idx < m_max_users; idx++) {
            waiter_order[idx] = -1;
        }
    }
    // Shuffle the order to randomize the wakeup order.
    std::shuffle(waiter_order.begin(), waiter_order.end(), std::default_random_engine());

    // Copy the order to the inactive array.  We do not shuffle in-place because RAtomics are
    // not move constructible, which is a requirement for std::shuffle.
    auto &waiter_order_to_modify = (m_wake_order_active == 0) ? m_wake_order_1 : m_wake_order_0;
    std::copy(waiter_order.begin(), waiter_order.end(), waiter_order_to_modify.begin());

    // Set the array we just modified to be the active one.  Since this is a relaxed write, it could take
    // some time for other CPUs to see the change; that's OK as this is all stochastic anyway.
    m_wake_order_active = (m_wake_order_active + 1) % 2;

    m_waiter_offset = 0;

    // If we find ourselves below the concurrency limit because we woke up too few operations in the last
    // interval, try waking up enough operations to fill the gap.  If we race with new incoming operations,
    // the threads will just go back to sleep.
    if (users_with_waiters) {
        m_waiting_users = users_with_waiters;
        auto io_active = m_io_active.load(std::memory_order_acquire);
        for (size_t idx = io_active; idx < static_cast<size_t>(m_concurrency_limit); idx++) {
            NotifyOne();
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
      // The connection counter can accumulate a number of known-idle connections.
      // We only need to keep long-term memory of idle ones.  Take this chance to garbage
      // collect old connection counters.
      if (m_max_open || m_max_conns) {
          const std::lock_guard<std::mutex> lock(m_file_mutex);
          for (auto iter = m_active_conns.begin(); iter != m_active_conns.end();)
          {
              auto & conn_count = *iter;
              if (!conn_count.second) {
                  iter = m_active_conns.erase(iter);
                  continue;
              }
              for (auto iter2 = conn_count.second->begin(); iter2 != conn_count.second->end();) {
                  if (iter2->second == 0) {
                      iter2 = conn_count.second->erase(iter2);
                  } else {
                      iter2++;
                  }
              }
              if (!conn_count.second->size()) {
                  iter = m_active_conns.erase(iter);
              } else {
                  iter++;
              }
          }
          for (auto iter = m_conn_counters.begin(); iter != m_conn_counters.end();) {
              if (!iter->second) {
                  iter = m_conn_counters.erase(iter);
              } else {
                  iter++;
              }
          }
          for (auto iter = m_file_counters.begin(); iter != m_file_counters.end();) {
              if (!iter->second) {
                  iter = m_file_counters.erase(iter);
              } else {
                  iter++;
              }
          }
      }

      TRACE(DEBUG, "Recomputing fairshares for throttle.");
      RecomputeInternal();
      ComputeWaiterOrder();
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

   AtomicEnd(m_compute_var);

   // Reset the loadshed limit counter.
   int limit_hit = m_loadshed_limit_hit.exchange(0);
   TRACE(DEBUG, "Throttle limit hit " << limit_hit << " times during last interval.");

   // Update the IO counters
   m_compute_var.Lock();
   m_stable_io_active = m_io_active.load(std::memory_order_acquire);
   auto io_active = m_stable_io_active;
   m_stable_io_total = m_io_total;
   auto io_total = m_stable_io_total;
   auto io_wait_rep = m_io_active_time.exchange(std::chrono::steady_clock::duration(0).count());
   m_stable_io_wait += std::chrono::steady_clock::duration(io_wait_rep);

   m_compute_var.UnLock();

   auto io_wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_stable_io_wait).count();
   TRACE(IOLOAD, "Current IO counter is " << io_active << "; total IO active time is " << io_wait_ms << "ms.");
   if (m_gstream)
   {
        char buf[128];
        auto len = snprintf(buf, 128,
                            R"({"event":"throttle_update","io_wait":%.4f,"io_active":%d,"io_total":%llu})",
                            static_cast<double>(io_wait_ms) / 1000.0, io_active, static_cast<long long unsigned>(io_total));
        auto suc = (len < 128) ? m_gstream->Insert(buf, len + 1) : false;
        if (!suc)
        {
            TRACE(IOLOAD, "Failed g-stream insertion of throttle_update record (len=" << len << "): " << buf);
        }
   }
   m_compute_var.Broadcast();
}

/*
 * Do a simple hash across the username.
 */
uint16_t
XrdThrottleManager::GetUid(const std::string &username)
{
    std::hash<std::string> hash_fn;
    auto hash = hash_fn(username);
    auto uid = static_cast<uint16_t>(hash % m_max_users);
    TRACE(DEBUG, "Mapping user " << username << " to UID " << uid);
    return uid;
}

/*
 * Notify a single waiter thread that it can proceed.
 */
void
XrdThrottleManager::NotifyOne()
{
    auto &wake_order = (m_wake_order_active == 0) ? m_wake_order_0 : m_wake_order_1;

    for (size_t idx = 0; idx < m_max_users; ++idx)
    {
        auto offset = m_waiter_offset.fetch_add(1, std::memory_order_acq_rel);
        int16_t uid = wake_order[offset % m_max_users];
        if (uid < 0)
        {
            continue;
        }
        auto &waiter_info = m_waiter_info[uid];
        std::unique_lock<std::mutex> lock(waiter_info.m_mutex);
        if (waiter_info.m_waiting) {
            waiter_info.NotifyOne(std::move(lock));
            return;
        }
   }
}

/*
 * Create an IO timer object; increment the number of outstanding IOs.
 */
XrdThrottleTimer
XrdThrottleManager::StartIOTimer(uint16_t uid, bool &ok)
{
   int cur_counter = m_io_active.fetch_add(1, std::memory_order_acq_rel);
   m_io_total++;

   while (m_concurrency_limit >= 0 && cur_counter >= m_concurrency_limit)
   {
      // If the user has essentially no concurrency, then we let them
      // temporarily exceed the limit.  This prevents potential waits for
      // every single read for an infrequent user.
      if (m_waiter_info[uid].m_concurrency < 1)
      {
         break;
      }
      m_loadshed_limit_hit++;
      m_io_active.fetch_sub(1, std::memory_order_acq_rel);
      TRACE(DEBUG, "ThrottleManager (user=" << uid << "): IO concurrency limit hit; waiting for other IOs to finish.");
      ok = m_waiter_info[uid].Wait();
      if (!ok) {
        TRACE(DEBUG, "ThrottleManager (user=" << uid << "): timed out waiting for other IOs to finish.");
        return XrdThrottleTimer();
      }
      cur_counter = m_io_active.fetch_add(1, std::memory_order_acq_rel);
   }

   ok = true;
   return XrdThrottleTimer(this, uid);
}

/*
 * Finish recording an IO timer.
 */
void
XrdThrottleManager::StopIOTimer(std::chrono::steady_clock::duration & event_duration, uint16_t uid)
{
   m_io_active_time += event_duration.count();
   auto old_active = m_io_active.fetch_sub(1, std::memory_order_acq_rel);
   m_waiter_info[uid].m_io_time += event_duration.count();
   if (old_active == static_cast<unsigned>(m_concurrency_limit))
   {
      // If we are below the concurrency limit threshold and have another waiter
      // for our user, then execute it immediately.  Otherwise, we will give
      // someone else a chance to run (as we have gotten more than our share recently).
      unsigned waiting_users = m_waiting_users;
      if (waiting_users == 0) waiting_users = 1;
      if (m_waiter_info[uid].m_concurrency < m_concurrency_limit / waiting_users)
      {
         std::unique_lock<std::mutex> lock(m_waiter_info[uid].m_mutex);
         if (m_waiter_info[uid].m_waiting > 0)
         {
            m_waiter_info[uid].NotifyOne(std::move(lock));
            return;
         }
      }
      NotifyOne();
   }
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
   if (m_loadshed_limit_hit == 0)
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

bool
XrdThrottleManager::Waiter::Wait()
{
    auto timeout = std::chrono::steady_clock::now() + m_manager->m_max_wait_time;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_waiting++;
        m_cv.wait_until(lock, timeout,
                        [&] { return m_manager->m_io_active.load(std::memory_order_acquire) < static_cast<unsigned>(m_manager->m_concurrency_limit) || std::chrono::steady_clock::now() >= timeout; });
        m_waiting--;
    }
    if (std::chrono::steady_clock::now() > timeout) {
        return false;
    }
    return true;
}

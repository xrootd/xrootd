
/*
 * XrdThrottleManager
 *
 * This class provides an implementation of a throttle manager.
 * The throttled manager purposely pause if the bandwidth, IOPS
 * rate, or number of outstanding IO requests  is sustained above 
 * a certain level.
 *
 * The XrdThrottleManager is user-aware and provides fairshare.
 *
 * This works by having a separate thread periodically refilling
 * each user's shares.
 *
 * Note that we do not actually keep close track of users, but rather
 * put them into a hash.  This way, we can pretend there's a constant
 * number of users and use a lock-free algorithm.
 */

#ifndef __XrdThrottleManager_hh_
#define __XrdThrottleManager_hh_

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       x
#define unlikely(x)     x
#endif

#include <array>
#include <ctime>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "XrdSys/XrdSysRAtomic.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdSecEntity;
class XrdSysError;
class XrdOucTrace;
class XrdThrottleTimer;
class XrdXrootdGStream;

namespace XrdThrottle {
   class Configuration;
}

class XrdThrottleManager
{

friend class XrdThrottleTimer;

public:

void        Init();

bool        OpenFile(const std::string &entity, std::string &open_error_message);
bool        CloseFile(const std::string &entity);

void        Apply(int reqsize, int reqops, int uid);

void        FromConfig(XrdThrottle::Configuration &config);

bool        IsThrottling() {return (m_ops_per_second > 0) || (m_bytes_per_second > 0);}

// Returns the user name and UID for the given client.
//
// The UID is a hash of the user name; it is not guaranteed to be unique.
std::tuple<std::string, uint16_t> GetUserInfo(const XrdSecEntity *client);

void        SetThrottles(float reqbyterate, float reqoprate, int concurrency, float interval_length)
            {m_interval_length_seconds = interval_length; m_bytes_per_second = reqbyterate;
             m_ops_per_second = reqoprate; m_concurrency_limit = concurrency;}

void        SetLoadShed(std::string &hostname, unsigned port, unsigned frequency)
            {m_loadshed_host = hostname; m_loadshed_port = port; m_loadshed_frequency = frequency;}

void        SetMaxOpen(unsigned long max_open) {m_max_open = max_open;}

void        SetMaxConns(unsigned long max_conns) {m_max_conns = max_conns;}

void        SetMaxWait(unsigned long max_wait) {m_max_wait_time = std::chrono::seconds(max_wait);}

// Load per-user limits from configuration file
// Returns 0 on success, non-zero on failure
int         LoadUserLimits(const std::string &config_file);

// Reload per-user limits (for runtime reloading)
int         ReloadUserLimits();

// Get per-user connection limit for a given username
// Returns 0 if no per-user limit is set (use global), otherwise returns the limit
unsigned long GetUserMaxConn(const std::string &username);

void        SetMonitor(XrdXrootdGStream *gstream) {m_gstream = gstream;}

//int         Stats(char *buff, int blen, int do_sync=0) {return m_pool.Stats(buff, blen, do_sync);}

// Notify that an I/O operation has started for a given user.
//
// If we are at the maximum concurrency limit then this will block;
// if we block for too long, the second return value will return false.
XrdThrottleTimer StartIOTimer(uint16_t uid, bool &ok);

void        PrepLoadShed(const char *opaque, std::string &lsOpaque);

bool        CheckLoadShed(const std::string &opaque);

void        PerformLoadShed(const std::string &opaque, std::string &host, unsigned &port);

            XrdThrottleManager(XrdSysError *lP, XrdOucTrace *tP);

           ~XrdThrottleManager() {} // The buffmanager is never deleted

protected:

// Notify the manager an I/O operation has completed for a given user.
// This is used to update the I/O wait time for the user and, potentially,
// wake up a waiting thread.
void        StopIOTimer(std::chrono::steady_clock::duration & event_duration, uint16_t uid);

private:

// Determine the UID for a given user name.
// This is a hash of the username; it is not guaranteed to be unique.
// The UID is used to index into the waiters array and cannot be more than m_max_users.
uint16_t    GetUid(const std::string &);

void        Recompute();

void        RecomputeInternal();

static
void *      RecomputeBootstrap(void *pp);

// Compute the order of wakeups for the existing waiters.
void ComputeWaiterOrder();

// Walk through the outstanding IO operations and compute the per-user
// IO time.
//
// Meant to be done periodically as part of the Recompute interval.  Used
// to make sure we have a better estimate of the concurrency for each user.
void UserIOAccounting();

int         WaitForShares();

void        GetShares(int &shares, int &request);

void        StealShares(int uid, int &reqsize, int &reqops);

// Return the timer hash list ID to use for the current request.
//
// When on Linux, this will hash across the CPU ID; the goal is to distribute
// the different timers across several lists to avoid mutex contention.
static unsigned GetTimerListHash();

// Notify a single waiter thread that it can proceed.
void NotifyOne();

XrdOucTrace * m_trace;
XrdSysError * m_log;

XrdSysCondVar m_compute_var;

// Controls for the various rates.
float       m_interval_length_seconds;
float       m_bytes_per_second;
float       m_ops_per_second;
int         m_concurrency_limit;

// Maintain the shares

static constexpr int m_max_users = 1024; // Maximum number of users we can have; used for various fixed-size arrays.
std::vector<int> m_primary_bytes_shares;
std::vector<int> m_secondary_bytes_shares;
std::vector<int> m_primary_ops_shares;
std::vector<int> m_secondary_ops_shares;
int         m_last_round_allocation;

// Waiter counts for each user
struct alignas(64) Waiter
{
   std::condition_variable m_cv; // Condition variable for waiters of this user.
   std::mutex m_mutex; // Mutex for this structure
   unsigned m_waiting{0}; // Number of waiting operations for this user.

   // EWMA of the concurrency for this user.  This is used to determine how much
   // above / below the user's concurrency share they've been recently.  This subsequently
   // will affect the likelihood of being woken up.
   XrdSys::RAtomic<float> m_concurrency{0};

   // I/O time for this user since the last recompute interval.  The value is used
   // to compute the EWMA of the concurrency (m_concurrency).
   XrdSys::RAtomic<std::chrono::steady_clock::duration::rep> m_io_time{0};

   // Pointer to the XrdThrottleManager object that owns this waiter.
   XrdThrottleManager *m_manager{nullptr};

   // Causes the current thread to wait until it's the user's turn to wake up.
   bool Wait();

   // Wakes up one I/O operation for this user.
   void NotifyOne(std::unique_lock<std::mutex> lock)
   {
      m_cv.notify_one();
   }
};
std::array<Waiter, m_max_users> m_waiter_info;

// Array with the wake up ordering of the waiter users.
// Every recompute interval, we compute how much over the concurrency limit
// each user is, quantize this to an integer number of shares and then set the
// array value to the user ID (so if user ID 5 has two shares, then there are two
// entries with value 5 in the array).  The array is then shuffled to randomize the
// order of the wakeup.
//
// All reads and writes to the wake order array are meant to be relaxed atomics; if a thread
// has an outdated view of the array, it simply means that a given user might get slightly
// incorrect random probability of being woken up.  That's seen as acceptable to keep
// the selection algorithm lock and fence-free.
std::array<XrdSys::RAtomic<int16_t>, m_max_users> m_wake_order_0;
std::array<XrdSys::RAtomic<int16_t>, m_max_users> m_wake_order_1; // A second wake order array; every recompute interval, we will swap the active array, avoiding locks.
XrdSys::RAtomic<char> m_wake_order_active; // The current active wake order array; 0 or 1
std::atomic<size_t> m_waiter_offset{0}; // Offset inside the wake order array; this is used to wake up the next potential user in line.  Cannot be relaxed atomic as offsets need to be seen in order.
std::chrono::steady_clock::time_point m_last_waiter_recompute_time; // Last time we recomputed the wait ordering.
XrdSys::RAtomic<unsigned> m_waiting_users{0}; // Number of users waiting behind the throttle as of the last recompute time.

std::atomic<uint32_t> m_io_active; // Count of in-progress IO operations: cannot be a relaxed atomic as ordering of inc/dec matters.
XrdSys::RAtomic<std::chrono::steady_clock::duration::rep> m_io_active_time; // Total IO wait time recorded since the last recompute interval; reset to zero about every second.
XrdSys::RAtomic<uint64_t> m_io_total{0}; // Monotonically increasing count of IO operations; reset to zero about every second.

int m_stable_io_active{0}; // Number of IO operations in progress as of the last recompute interval; must hold m_compute_var lock when reading/writing.
uint64_t m_stable_io_total{0}; // Total IO operations since startup.  Recomputed every second; must hold m_compute_var lock when reading/writing.

std::chrono::steady_clock::duration m_stable_io_wait; // Total IO wait time as of the last recompute interval.

// Load shed details
std::string m_loadshed_host;
unsigned m_loadshed_port;
unsigned m_loadshed_frequency;

// The number of times we have an I/O operation that hit the concurrency limit.
// This is monotonically increasing and is "relaxed" because it's purely advisory;
// ordering of the increments between threads is not important.
XrdSys::RAtomic<int> m_loadshed_limit_hit;

// Maximum number of open files
unsigned long m_max_open{0};
unsigned long m_max_conns{0};
std::unordered_map<std::string, unsigned long> m_file_counters;
std::unordered_map<std::string, unsigned long> m_conn_counters;
std::unordered_map<std::string, std::unique_ptr<std::unordered_map<pid_t, unsigned long>>> m_active_conns;
std::mutex m_file_mutex;

// Per-user connection limits
struct UserLimit {
    unsigned long max_conn{0};  // 0 means no limit (use global)
    bool is_wildcard{false};    // true if this is a wildcard pattern
};
std::unordered_map<std::string, UserLimit> m_user_limits;
std::shared_mutex m_user_limits_mutex;
std::string m_user_config_file;

// Track the ongoing I/O operations.  We have several linked lists (hashed on the
// CPU ID) of I/O operations that are in progress.  This way, we can periodically sum
// up the time spent in ongoing operations - which is important for operations that
// last longer than the recompute interval.
struct TimerList {
   std::mutex m_mutex;
   XrdThrottleTimer *m_first{nullptr};
   XrdThrottleTimer *m_last{nullptr};
};
#if defined(__linux__)
static constexpr size_t m_timer_list_size = 32;
#else
static constexpr size_t m_timer_list_size = 1;
#endif
std::array<TimerList, m_timer_list_size> m_timer_list; // A vector of linked lists of I/O operations.  We keep track of multiple instead of a single one to avoid a global mutex.

// Maximum wait time for a user to perform an I/O operation before failing.
// Most clients have some sort of operation timeout; after that point, if we go
// ahead and do the work, it's wasted effort as the client has gone.
std::chrono::steady_clock::duration m_max_wait_time{std::chrono::seconds(30)};

// Monitoring handle, if configured
XrdXrootdGStream* m_gstream{nullptr};

static const char *TraceID;

};

class XrdThrottleTimer
{

friend class XrdThrottleManager;

public:

~XrdThrottleTimer()
{
   if (m_manager) {
      StopTimer();
   }
}

protected:

XrdThrottleTimer() :
   m_start_time(std::chrono::steady_clock::time_point::min())
{}

XrdThrottleTimer(XrdThrottleManager *manager, int uid) :
   m_owner(uid),
   m_timer_list_entry(XrdThrottleManager::GetTimerListHash()),
   m_manager(manager),
   m_start_time(std::chrono::steady_clock::now())
{
   if (!m_manager) {
      return;
   }
   auto &timerList = m_manager->m_timer_list[m_timer_list_entry];
   std::lock_guard<std::mutex> lock(timerList.m_mutex);
   if (timerList.m_first == nullptr) {
      timerList.m_first = this;
   } else {
      m_prev = timerList.m_last;
      m_prev->m_next = this;
   }
   timerList.m_last = this;
}

std::chrono::steady_clock::duration Reset() {
   auto now = std::chrono::steady_clock::now();
   auto last_start = m_start_time.exchange(now);
   return now - last_start;
}

private:

   void StopTimer()
   {
      if (!m_manager) return;

      auto event_duration = Reset();
      auto &timerList = m_manager->m_timer_list[m_timer_list_entry];
      {
         std::unique_lock<std::mutex> lock(timerList.m_mutex);
         if (m_prev) {
            m_prev->m_next = m_next;
            if (m_next) {
               m_next->m_prev = m_prev;
            } else {
               timerList.m_last = m_prev;
            }
         } else {
            timerList.m_first = m_next;
            if (m_next) {
               m_next->m_prev = nullptr;
            } else {
               timerList.m_last = nullptr;
            }
         }
      }
      m_manager->StopIOTimer(event_duration, m_owner);
   }

   const uint16_t m_owner{0};
   const uint16_t m_timer_list_entry{0};
   XrdThrottleManager *m_manager{nullptr};
   XrdThrottleTimer *m_prev{nullptr};
   XrdThrottleTimer *m_next{nullptr};
   XrdSys::RAtomic<std::chrono::steady_clock::time_point> m_start_time;

};

#endif

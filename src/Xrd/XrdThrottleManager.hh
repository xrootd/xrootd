
/*
 * XrdThrottleManager
 *
 * This class provides an implementation of a throttle manager.
 * The throttled manager purposely pause if the bandwidth or IOPS
 * rate is sustained above a certain level.
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

#include <vector>

#include "XrdSys/XrdSysPthread.hh"

class XrdSysError;
class XrdOucTrace;

class XrdThrottleManager
{

public:

void        Init();

void        Apply(int reqsize, int reqops, int uid);

bool        IsThrottling() {return (m_ops_per_second > 0) || (m_bytes_per_second > 0);}

void        SetThrottles(float reqbyterate, float reqoprate, float interval_length)
            {m_interval_length_seconds = interval_length; m_bytes_per_second = reqbyterate;
             m_ops_per_second = reqoprate;}

//int         Stats(char *buff, int blen, int do_sync=0) {return m_pool.Stats(buff, blen, do_sync);}

static
int         GetUid(const char *username);

            XrdThrottleManager(XrdSysError *lP, XrdOucTrace *tP);

           ~XrdThrottleManager() {} // The buffmanager is never deleted

private:

void        Recompute();

void        RecomputeInternal();

static
void *      RecomputeBootstrap(void *pp);

int         WaitForShares();

void        GetShares(int &shares, int &request);

void        StealShares(int uid, int &reqsize, int &reqops);

XrdOucTrace * m_trace;
XrdSysError * m_log;

XrdSysCondVar m_compute_var;

// Controls for the various rates.
float       m_interval_length_seconds;
float       m_bytes_per_second;
float       m_ops_per_second;

// Maintain the shares
static const
int         m_max_users;
std::vector<int> m_primary_bytes_shares;
std::vector<int> m_secondary_bytes_shares;
std::vector<int> m_primary_ops_shares;
std::vector<int> m_secondary_ops_shares;
int         m_last_round_allocation;

static const char *TraceID;

};

#endif

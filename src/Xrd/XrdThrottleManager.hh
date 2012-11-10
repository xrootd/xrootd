
/*
 * XrdThrottleManager
 *
 * This class provides an implementation of a throttled buffer
 * manager.  The throttled manager will only give out a buffer at
 * particular rate, both in terms of IOPS and bandwidth.
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
#include "Xrd/XrdBuffer.hh"

class XrdThrottleManager
{

public:

void        Init();

XrdBuffer  *Obtain(int bsz, int reqsize, int reqops, int uid);

int         Recalc(int bsz);

void        Release(XrdBuffer *bp);

int         MaxSize() {return m_pool.MaxSize();}

void        Set(int reqbyterate, int reqoprate, int interval=1, int maxmem=-1, int minw=-1);

int         Stats(char *buff, int blen, int do_sync=0) {return m_pool.Stats(buff, blen, do_sync);}

int         GetUid(char *username);

            XrdThrottleManager(XrdSysError *lP, XrdOucTrace *tP, int minrst=20*60);

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

XrdBuffManager m_pool;

XrdSysCondVar m_compute_var;

// Controls for the various rates.
float       m_interval_length_seconds;
float       m_bytes_per_second;
float       m_ops_per_second;

// Maintain the shares
int         m_max_users;
std::vector<int> m_primary_bytes_shares;
std::vector<int> m_secondary_bytes_shares;
std::vector<int> m_primary_ops_shares;
std::vector<int> m_secondary_ops_shares;
int         m_last_round_allocation;

};

#endif

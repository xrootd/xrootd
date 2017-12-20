#ifndef __SSI_SHMAM__
#define __SSI_SHMAM__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S h M a m . h h                         */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <pthread.h>

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiShMat.hh"

class XrdSsiShMam : public XrdSsiShMat
{
public:

bool AddItem(void *newdata, void *olddata, const char *key,
             int   hash,    bool  replace=false);

bool Attach(int tout, bool isrw=false);

bool Create(XrdSsiShMat::CRZParms &parms);

bool Export();

bool DelItem(void *data, const char *key, int hash);

void Detach();

bool Enumerate(void *&jar, char *&key, void *&val);

bool Enumerate(void *&jar);

bool GetItem(void *data, const char *key, int hash);

int  Info(const char *vname, char *buff=0, int blen=0);

bool Resize(XrdSsiShMat::CRZParms &parms);

bool Sync();
bool Sync(bool dosync, bool syncdo);
bool Sync(int syncqsz);

     XrdSsiShMam(XrdSsiShMat::NewParms &parms);

    ~XrdSsiShMam() {Detach();
                    pthread_mutex_destroy(&lkMutex);
                    pthread_rwlock_destroy(&myMutex);
                   }

enum LockType {ROLock= 0, RWLock = 1};

private:
struct   MemItem {int hash; Atomic(int) next;};

bool     ExportIt(bool fLocked);
int      Find(MemItem  *&theItem, MemItem *&prvItem, const char *key, int &hash);
bool     Flush();
int      HashVal(const char *key);
bool     Lock(bool doRW=false, bool nowait=false);
MemItem *NewItem();
bool     ReMap(LockType iHave);
void     RetItem(MemItem *iP);
void     SetLocking(bool isrw);
void     SwapMap(XrdSsiShMam &newMap);
void     Snooze(int sec);
void     UnLock(bool isrw);
void     Updated(int mOff);
void     Updated(int mOff, int  mLen);

class XLockHelper
{
public:
inline bool  FLock() {if (!(shmemP->Lock(lkType))) return false;
                      doUnLock = true; return true;
                     }

             XLockHelper(XrdSsiShMam *shmemp, LockType lktype)
                        : shmemP(shmemp), lkType(lktype), doUnLock(false)
                        {if (lktype == RWLock)
                                 pthread_rwlock_wrlock(&(shmemP->myMutex));
                            else pthread_rwlock_rdlock(&(shmemP->myMutex));
                        }
            ~XLockHelper() {int rc = errno;
                            if (lkType == RWLock && shmemP->syncOn
                            &&  shmemP->syncQWR > shmemP->syncQSZ)
                               shmemP-> Flush();
                            if (doUnLock) shmemP->UnLock(lkType == RWLock);
                            pthread_rwlock_unlock(&(shmemP->myMutex));
                            errno = rc;
                           }
private:
XrdSsiShMam *shmemP;
LockType     lkType;
bool         doUnLock;
};

pthread_mutex_t   lkMutex;
pthread_rwlock_t  myMutex;

char       *shmTemp;
long long   shmSize;
char       *shmBase;
Atomic(int)*shmIndex;
int         shmSlots;
int         shmItemSz;
int         shmInfoSz;
int         verNum;
int         keyPos;
int         maxKLen;
int         shmFD;
int         timeOut;
int         lkCount;
int         syncOpt;
int         syncQWR;
int         syncLast;
int         syncQSZ;
int         accMode;
bool        isRW;
bool        lockRO;
bool        lockRW;
bool        reUse;
bool        multW;
bool        useAtomic;
bool        syncBase;
bool        syncOn;
};
#endif

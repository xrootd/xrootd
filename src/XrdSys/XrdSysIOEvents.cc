/******************************************************************************/
/*                                                                            */
/*                     X r d S y s I O E v e n t s . c c                      */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
  
#include "XrdSys/XrdSysIOEvents.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define SINGLETON(dlvar, theitem)\
               theitem ->dlvar .next == theitem
  
#define INSERT(dlvar, curitem, newitem) \
               newitem ->dlvar .next = curitem; \
               newitem ->dlvar .prev = curitem ->dlvar .prev; \
               curitem ->dlvar .prev-> dlvar .next = newitem; \
               curitem ->dlvar .prev = newitem

#define REMOVE(dlbase, dlvar, curitem) \
               if (dlbase == curitem) dlbase = (SINGLETON(dlvar,curitem) \
                                             ? 0   : curitem ->dlvar .next);\
               curitem ->dlvar .prev-> dlvar .next = curitem ->dlvar .next;\
               curitem ->dlvar .next-> dlvar .prev = curitem ->dlvar .prev;\
               curitem ->dlvar .next = curitem;\
               curitem ->dlvar .prev = curitem

#define REVENTS(x) x & Channel:: readEvents

#define WEVENTS(x) x & Channel::writeEvents

#define ISPOLLER XrdSysThread::Same(XrdSysThread::ID(),pollTid)

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/
  
       time_t       XrdSys::IOEvents::Poller::maxTime
                    = (sizeof(time_t) == 8 ? 0x7fffffffffffffffLL : 0x7fffffff);
 
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*              T h r e a d   S t a r t u p   I n t e r f a c e               */
/******************************************************************************/

namespace XrdSys
{
namespace IOEvents
{
struct pollArg
       {Poller         *pollP;
        const char     *retMsg;
        int             retCode;
        XrdSysSemaphore pollSync;

        pollArg() : retMsg(0), retCode(0), pollSync(0, "poll sync") {}
       ~pollArg() {}
       };

class BootStrap
{
public:
  
static void *Start(void *parg);
};

void *BootStrap::Start(void *parg)
{
   struct pollArg *pollArg = (struct pollArg *)parg;
   Poller *thePoller = pollArg->pollP;
   thePoller->pollTid = XrdSysThread::ID();

   thePoller->Begin(&(pollArg->pollSync), pollArg->retCode, &(pollArg->retMsg));

   return (void *)0;
}

/******************************************************************************/
/*                            P o l l e r E r r 1                             */
/******************************************************************************/

// This class is set in the channel when an error occurs but cannot be reflected
// immediately because either there is no callback function or all events are
// disabled. We need to do this because error events can be physically presented
// by the kernel even when logical events are disabled. Note that the error
// number and text will have been set and remain set as the channel was actually
// disabled preventing any new operation on the channel.
//
class PollerErr1 : public Poller
{
public:

     PollerErr1() : Poller(-1, -1) {}
    ~PollerErr1() {}

protected:
void Begin(XrdSysSemaphore *syncp, int &rc, const char **eTxt) {}

void Exclude(Channel *cP, bool &isLocked, bool dover=1) {}

bool Include(Channel *cP, int &eNum, const char **eTxt, bool &isLocked)
            {if (!(eNum = GetFault(cP))) eNum = EPROTO;
             if (eTxt) *eTxt = "initializing channel";
             return false;
            }

bool Modify (Channel *cP, int &eNum, const char **eTxt, bool &isLocked)
            {if (!(eNum = GetFault(cP))) eNum = EPROTO;
             if (eTxt) *eTxt = "modifying channel";
             return false;
            }

void Shutdown() {}
};

/******************************************************************************/
/*                            P o l l e r I n i t                             */
/******************************************************************************/
  
// This class is used as the initial poller on a channel. It is responsible
// for adding the file descriptor to the poll set upon the initial enable. This
// avoids enabling a channel prior to it receiving a call back function.
//
class PollerInit : public Poller
{
public:

     PollerInit() : Poller(-1, -1) {}
    ~PollerInit() {}

protected:

void Begin(XrdSysSemaphore *syncp, int &rc, const char **eTxt) {}

void Exclude(Channel *cP, bool &isLocked, bool dover=1) {}

bool Include(Channel *cP, int &eNum, const char **eTxt, bool &isLocked)
            {eNum = EPROTO;
             if (eTxt) *eTxt = "initializing channel";
             return false;
            }

bool Modify (Channel *cP, int &eNum, const char **eTxt, bool &isLocked)
            {return Init(cP, eNum, eTxt, isLocked);}

void Shutdown() {}
};
  
/******************************************************************************/
/*                            P o l l e r W a i t                             */
/******************************************************************************/
  
// This class is set in the channel when we need to serialize aces to the
// channel. Channel methods (as some others) check for this to see if they need
// to defer the current operation. We need to do his because some poller
// implementations must release the channel lock to avoid a deadlock.
//
class PollerWait : public Poller
{
public:

     PollerWait() : Poller(-1, -1) {}
    ~PollerWait() {}

protected:
void Begin(XrdSysSemaphore *syncp, int &rc, const char **eTxt) {}

void Exclude(Channel *cP, bool &isLocked, bool dover=1) {}

bool Include(Channel *cP, int &eNum, const char **eTxt, bool &isLocked)
            {eNum = EIDRM;
             if (eTxt) *eTxt = "initializing channel";
             return false;
            }

bool Modify (Channel *cP, int &eNum, const char **eTxt, bool &isLocked)
            {eNum = EDEADLK;
             if (eTxt) *eTxt = "modifying channel";
             return false;
            }

void Shutdown() {}
};

PollerErr1 pollErr1;
PollerInit pollInit;
PollerInit pollWait;
};
};

/******************************************************************************/
/*                 C l a s s   C h a n n e l   M e t h o d s                  */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSys::IOEvents::Channel::Channel(Poller   *pollP, int   fd,
                                   CallBack *cbP, void *cbArg)
                          : chPollXQ(pollP), chCB(cbP), chCBA(cbArg)
{
   attList.next = attList.prev = this;
   tmoList.next = tmoList.prev = this;
   inTOQ    = 0;
   pollEnt  = 0;
   chStat   = isClear;
   Reset(&pollInit, fd);

   pollP->Attach(this);
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdSys::IOEvents::Channel::~Channel()
{
   Poller *myPoller;
   bool isLocked = true;

// Lock ourselves during the delete process. If the channel is disassociated
// then we need do nothing more.
//
   chMutex.Lock();
   if (!chPollXQ || chPollXQ == &pollErr1)
      {chMutex.UnLock();
       return;
      }

// Disable and remove ourselves from all queues
//
   myPoller = chPollXQ;
   chPollXQ->Detach(this,isLocked,false);
   if (!isLocked) chMutex.Lock();

// If we are in callback mode then we will need to delay the destruction until
// after the callback completes unless this is the poller thread. In that case,
// we need to tell the poller that we have been destroyed in a shelf-stable way.
//
   if (chStat)
      {if (XrdSysThread::Same(XrdSysThread::ID(),myPoller->pollTid))
          {myPoller->chDead = true;
           chMutex.UnLock();
          } else {
           XrdSysSemaphore cbDone(0);
           chStat = isDead;
           chCBA  = (void *)&cbDone;
           chMutex.UnLock();
           cbDone.Wait();
          }
      }
}
  
/******************************************************************************/
/*                               D i s a b l e                                */
/******************************************************************************/
  
bool XrdSys::IOEvents::Channel::Disable(int events, const char **eText)
{
   int eNum, newev, curev;
   bool retval = true, isLocked = true;

// Lock this channel
//
   chMutex.Lock();

// Calculate new event mask
//
   curev   = static_cast<int>(chEvents);
   events &= allEvents;
   newev   = curev & ~events;

// If something has changed, then modify the event mask in the poller. The
// poller may or may not unlock this channel during the process.
//
   if (newev != curev)
      {chEvents = newev;
       retval = chPoller->Modify(this, eNum, eText, isLocked);
      }
   if (isLocked) chMutex.UnLock();

// All done
//
   if (!retval) errno = eNum;
   return retval;
}

/******************************************************************************/
/*                                E n a b l e                                 */
/******************************************************************************/
  
bool XrdSys::IOEvents::Channel::Enable(int events, int timeout,
                                       const char **eText)
{
   time_t newDL;
   int eNum, newev, curev = static_cast<int>(chEvents);
   bool retval, isLocked = true, setTO = false;

// Lock ourselves against any changes (this is a recursive mutex)
//
   chMutex.Lock();

// Establish events that should be enabled
//
   events &= allEvents;
   newev = (curev ^ events) & events;
   chEvents |= events;

// Handle timeout changes now
//
   if (timeout)
      {newDL = (timeout > 0 ? timeout + time(0) : Poller::maxTime);
       if (events & readEvents)
          {chRTO = (timeout < 0 ? 0 : timeout);
           if (newDL != rdDL) {rdDL  = newDL; setTO = true;}
          }
       if (events & writeEvents)
          {chWTO = (timeout < 0 ? 0 : timeout);
           if (newDL != wrDL) {wrDL  = newDL; setTO = true;}
          }
      } else {
       time_t nowTime = (chRTO || chWTO ? time(0) : 0);
       if (events & readEvents)
          {newDL = (chRTO ? nowTime + chRTO : Poller::maxTime);
           if (newDL != rdDL) {rdDL  = newDL; setTO = true;}
          }
       if (events & writeEvents)
          {newDL = (chWTO ? nowTime + chWTO : Poller::maxTime);
           if (newDL != wrDL) {wrDL  = newDL; setTO = true;}
          }
      }

// Check if we have to reset the timeout. We need to hold the channel lock here.
//
   if (setTO && (chPoller == &pollInit || chPoller != &pollErr1))
           setTO = chPollXQ->TmoAdd(this);
      else setTO = false;

// Check if any modifcations needed here. If so, invoke the modifier. Note that
// the modify will unlock the channel if the operation causes a wait. So,
// we cannot depend on the channel being locked upon return. The reason we do
// not unlock here is because we must ensure the channel doesn't change while
// we call modify. We let modify determine what to do.
//
   retval = (newev ? chPoller->Modify(this, eNum, eText, isLocked) : true);

// We need to notify the poller thread if the added deadline is the first in the
// queue and the poller is waiting. We also optimize for the case where the
// poller thread is always woken up to perform an action in which case it
// doesn't need a separate wakeup. We only do this if the enable succeeed. Note
// that we cannot hold the channel mutex for this call because it may wait.
//
   if (isLocked) chMutex.UnLock();
   if (retval && !(chPollXQ->wakePend) && setTO && isLocked) chPollXQ->WakeUp();

// All done
//
   if (!retval) errno = eNum;
   return retval;
}

/******************************************************************************/
/*                           G e t C a l l B a c k                            */
/******************************************************************************/
  
void XrdSys::IOEvents::Channel::GetCallBack(CallBack **cbP, void **cbArg)
{
   chMutex.Lock();
   *cbP   = chCB;
   *cbArg = chCBA;
   chMutex.UnLock();
}

/******************************************************************************/
/* Private:                        R e s e t                                  */
/******************************************************************************/
  
void XrdSys::IOEvents::Channel::Reset(XrdSys::IOEvents::Poller *thePoller,
                                      int fd, int eNum)
{
     chPoller = thePoller;
     chFD     = fd;
     chFault  = eNum;
     chRTO    = 0;
     chWTO    = 0;
     chEvents = 0;
     dlType   = 0;
     inPSet   = 0;
     rdDL     = Poller::maxTime;
     wrDL     = Poller::maxTime;
     deadLine = Poller::maxTime;
}

/******************************************************************************/
/*                           S e t C a l l B a c k                            */
/******************************************************************************/
  
void XrdSys::IOEvents::Channel::SetCallBack(CallBack *cbP, void *cbArg)
{

// We only need to have the channel lock to set the callback. If the object
// is in the process of being destroyed, we do nothing.
//
   chMutex.Lock();
   if (chStat != isDead)
      {chCB  = cbP;
       chCBA = cbArg;
      }
   chMutex.UnLock();
}

/******************************************************************************/
/*                                 S e t F D                                  */
/******************************************************************************/
  
void XrdSys::IOEvents::Channel::SetFD(int fd)
{
   bool isLocked = true;

// Obtain the channel lock. If the object is in callback mode we have some
// extra work to do. If normal callback then indicate the channel transitioned
// to prevent it being automatically re-enabled. If it's being destroyed, then
// do nothing. Otherwise, this is a stupid double setFD call.
//
   chMutex.Lock();
   if (chStat == isDead)
      {chMutex.UnLock();
       return;
      }

// This is a tricky deal here because we need to protect ourselves from other
// threads as well as the poller trying to do a callback. We first, set the
// poller target. This means the channel is no longer ready and callbacks will
// be skipped. We then remove the current file descriptor. This may unlock the
// channel but at this point that's ok.
//
   if (inPSet)
      {chPoller = &pollWait;
       chPollXQ->Detach(this, isLocked, true);
       if (!isLocked) chMutex.Lock();
      }

// Indicate channel needs to be re-enabled then unlock the channel
//
   Reset(&pollInit, fd);
   chMutex.UnLock();
}
  
/******************************************************************************/
/*                          C l a s s   P o l l e r                           */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSys::IOEvents::Poller::Poller(int rFD, int cFD)
{

// Now initialize local class members
//
   attBase         = 0;
   tmoBase         = 0;
   cmdFD           = cFD;
   reqFD           = rFD;
   wakePend        = false;
   pipeBuff        = 0;
   pipeBlen        = 0;
   pipePoll.fd     = rFD;
   pipePoll.events = POLLIN | POLLRDNORM;
}

/******************************************************************************/
/*                                A t t a c h                                 */
/******************************************************************************/
  
void XrdSys::IOEvents::Poller::Attach(XrdSys::IOEvents::Channel *cP)
{
   Channel *pcP;

// We allow only one attach at a time to simplify the processing
//
   adMutex.Lock();

// Chain this channel into the list of attached channels
//
   if ((pcP = attBase)) {INSERT(attList, pcP, cP);}
      else attBase = cP;

// All done
//
   adMutex.UnLock();
}

/******************************************************************************/
/*                                C b k T M O                                 */
/******************************************************************************/

void XrdSys::IOEvents::Poller::CbkTMO()
{
   Channel *cP;
   time_t tNow = time(0);
   bool disChannel;

// Process each element in the timeout queue, calling the callback function
// if the timeout has passed. As the timeout mutex is recursive, we can keep
// it throughout this code as only this thread will modify the timeout list.
//
   toMutex.Lock();
   while((cP = tmoBase) && cP->deadLine <= tNow)
        CbkXeq(cP, cP->dlType, 0, 0);
   toMutex.UnLock();
}

/******************************************************************************/
/*                                C b k X e q                                 */
/******************************************************************************/
  
bool XrdSys::IOEvents::Poller::CbkXeq(XrdSys::IOEvents::Channel *cP, int events,
                                      int eNum, const char *eTxt)
{
   XrdSysMutexHelper cbkMHelp(cP->chMutex);
   char oldEvents;
   int isRead = 0, isWrite = 0;
   bool cbok, isLocked = true;

// Remove this from the timeout queue if there and reset the deadlines based
// on the event we are reflecting. This separates read and write deadlines
//
   if (cP->inTOQ)
      {TmoDel(cP);
       isRead = (events & (CallBack::ReadyToRead  | CallBack:: ReadTimeOut));
       if (isRead)  cP->rdDL = maxTime;
       isWrite= (events & (CallBack::ReadyToWrite | CallBack::WriteTimeOut));
       if (isWrite) cP->wrDL = maxTime;
      }

// Verify that there is a callback here and the channel is ready. If not,
// disable this channel for the events being refelcted unless the event is a
// fatal error. In this case we need to abandon the channel since error events
// may continue to be generated as we can't always disable them.
//
   if (!(cP->chCB) || cP->chPoller != cP->chPollXQ)
      {if (eNum)
          {cP->chPoller = &pollErr1; cP->chFault = eNum;
           cP->inPSet   = 0;
           return false;
          }
       oldEvents = cP->chEvents;
       cP->chEvents = 0;
       Modify(cP, eNum, 0, isLocked);
       if (!isLocked) cP->chMutex.Lock();
       cP->chEvents = oldEvents;
       return true;
      }

// Resolve the problem where we get an error event but the channel wants them
// presented as a read or write event. If neither is possible then defer the
// error until the channel is enabled again.
//
   if (eNum)
      {if (cP->chEvents & Channel::errorEvents)
          {cP->chPoller = &pollErr1; cP->chFault = eNum;
           cP->chCB->Fatal(cP,cP->chCBA, eNum, eTxt);
           return false;
          }
            if (REVENTS(cP->chEvents)) events = CallBack::ReadyToRead;
       else if (WEVENTS(cP->chEvents)) events = CallBack::ReadyToWrite;
       else    {cP->chPoller = &pollErr1; cP->chFault = eNum;
                return false;
               }
      }

// Indicate that we are in callback mode then drop the channel lock and effect
// the callback. This allows the callback to freely manage locks.
//
   cP->chStat = Channel::isCBMode;
   chDead     = false;
   cP->chMutex.UnLock();
   cbok = cP->chCB->Event(cP,cP->chCBA, events);

// If channel destroyed by the callback, bail really fast. Otherwise, regain
// the channel lock.
//
   if (chDead) return true;
   cP->chMutex.Lock();

// If the channel is being destroyed; then another thread must have done so.
// Tell it the callback has finished and just return.
//
   if (cP->chStat != Channel::isCBMode)
      {if (cP->chStat == Channel::isDead)
          ((XrdSysSemaphore *)cP->chCBA)->Post();
       return true;
      }
   cP->chStat = Channel::isClear;

// Handle enable or disable here. If we keep the channel enabled then reset
// the timeout if it hasn't been handled via a call from the callback.
//
        if (!cbok) Detach(cP,isLocked,false);
   else if (!(cP->inTOQ) && (cP->chRTO || cP->chWTO))
           {time_t tNow = time(0);
            if (isRead)  cP->rdDL = (REVENTS(cP->chEvents) && cP->chRTO
                                  ?  cP->chRTO + tNow : maxTime);
            if (isWrite) cP->wrDL = (WEVENTS(cP->chEvents) && cP->chWTO
                                  ?  cP->chWTO + tNow : maxTime);
            if (cP->rdDL != maxTime || cP->wrDL != maxTime) TmoAdd(cP);
           }

// All done. While the mutex should not have been unlocked, we relock it if
// it has to keep the mutex helper from croaking.
//
   if (!isLocked) cP->chMutex.Lock();
   return true;
}

/******************************************************************************/
/* Static:                        C r e a t e                                 */
/******************************************************************************/
  
XrdSys::IOEvents::Poller *XrdSys::IOEvents::Poller::Create(int         &eNum,
                                                           const char **eTxt)
{
   int i;
   int fildes[2];
   struct pollArg pArg;
   pthread_t tid;

// Create a pipe used to break the poll wait loop
//
   if (pipe(fildes))
      {eNum = errno;
       if (eTxt) *eTxt = "creating poll pipe";
       return 0;
      }
   fcntl(fildes[0], F_SETFD, FD_CLOEXEC);
   fcntl(fildes[1], F_SETFD, FD_CLOEXEC);

// Create an actual implementation of a poller
//
   if (!(pArg.pollP = newPoller(fildes, eNum, eTxt)))
      {close(fildes[0]);
       close(fildes[1]);
       return 0;
      }

// Now start a thread to handle this poller object
//
   if ((eNum = XrdSysThread::Run(&tid, XrdSys::IOEvents::BootStrap::Start,
                                 (void *)&pArg, XRDSYSTHREAD_BIND, "Poller")))
      {if (eTxt) *eTxt = "creating poller thread"; return 0;}

// Now wait for the thread to finish initializing before we allow use
//
   pArg.pollSync.Wait();

// Check if all went well
//
   if (pArg.retCode)
      {if (eTxt) *eTxt = (pArg.retMsg ? pArg.retMsg : "starting poller");
       eNum = pArg.retCode;
       delete pArg.pollP;
       return 0;
      }

// All done
//
   eNum = 0;
   if (eTxt) *eTxt = "";
   return pArg.pollP;
}
  
/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/
  
void XrdSys::IOEvents::Poller::Detach(XrdSys::IOEvents::Channel *cP,
                                      bool &isLocked, bool keep)
{
   bool detFD = (cP->inPSet != 0);

// First remove the channel from the timeout queue
//
   if (cP->inTOQ)
      {toMutex.Lock();
       REMOVE(tmoBase, tmoList, cP);
       toMutex.UnLock();
      }

// Allow only one detach at a time
//
   adMutex.Lock();

// Preset channel to prohibit callback if we are not keeping this channel
//
   if (!keep)
      {cP->Reset(&pollErr1, cP->chFD);
       cP->chPollXQ = &pollErr1;
       cP->chCB     = 0;
       cP->chCBA    = 0;
       if (cP->attList.next != cP) {REMOVE(attBase, attList, cP);}
          else if (attBase == cP) attBase = 0;
      }

// Exclude this channel from the associated poll set, don't hold the ad lock
//
   adMutex.UnLock();
   if (detFD && cmdFD >= 0) Exclude(cP, isLocked, !ISPOLLER);
}
  
/******************************************************************************/
/* Protected:                 G e t R e q u e s t                             */
/******************************************************************************/

// Warning: This method runs unlocked. The caller must have exclusive use of
//          the reqBuff otherwise unpredictable results will occur.

int XrdSys::IOEvents::Poller::GetRequest()
{
   ssize_t rlen;
   int rc;

// See if we are to resume a read or start a fresh one
//
   if (!pipeBlen) 
      {pipeBuff = (char *)&reqBuff; pipeBlen = sizeof(reqBuff);}

// Wait for the next request. Some OS's (like Linux) don't support non-blocking
// pipes. So, we must front the read with a poll.
//
   do {rc = poll(&pipePoll, 1, 0);}
      while(rc < 0 && (errno == EAGAIN || errno == EINTR));
   if (rc < 1) return 0;

// Now we can put up a read without a delay. Normally a full command will be
// present. Under some heavy conditions, this may not be the case.
//
   do {rlen = read(reqFD, pipeBuff, pipeBlen);} 
      while(rlen < 0 && errno == EINTR);
   if (rlen <= 0)
      {cerr <<"Poll: " <<strerror(errno) <<" reading from request pipe" <<endl;
       return 0;
      }

// Check if all the data has arrived. If not all the data is present, defer
// this request until more data arrives.
//
   if (!(pipeBlen -= rlen)) return 1;
   pipeBuff += rlen;
   return 0;
}
  
/******************************************************************************/
/* Protected:                       I n i t                                   */
/******************************************************************************/
  
bool XrdSys::IOEvents::Poller::Init(XrdSys::IOEvents::Channel *cP, int &eNum,
                                    const char **eTxt, bool &isLocked)
{
// The channel must be locked upon entry!
//
   bool retval;
   char oldEv;

// If no events are enabled at this point, just return
//
   if (!(cP->chEvents)) return true;

// Refuse to enable a channel without a callback function
//
   if (!(cP->chCB))
      {eNum = EDESTADDRREQ;
       if (eTxt) *eTxt = "enabling without a callback";
       return false;
      }

// If we are already in progress then indicate to the other thread that it
// needs to remodify this channel w.r.t. to the poller (this is very rare).
//
   if (cP->chPoller == &pollWait)
      {cP->reMod = 1;
       return true;
      }

// So, now we can include the channel in the poll set, enable it, and if
// successful, point it to a functioning poller.
//
   cP->chPoller = &pollWait; oldEv = cP->chEvents;
   retval = cP->chPollXQ->Include(cP, eNum, eTxt, isLocked);
   if (!isLocked) {cP->chMutex.Lock(); isLocked = true;}

// Determine what future poller to use and whether something happened should we
// have lost control of the channel. If something meaningful did happen then
// we need to redo it at this point as the other thread didn't want to wait.
//
   if (!retval) {cP->chPoller = &pollErr1; cP->chFault = eNum;}
      else {cP->chPoller = cP->chPollXQ;
            cP->inPSet   = 1;
            if (cP->reMod && cP->chEvents != oldEv)
               {retval = cP->chPoller->Modify(cP, eNum, eTxt, isLocked);
                if (!isLocked) {cP->chMutex.Lock(); isLocked = true;}
               }
           }

// All done
//
   cP->reMod = 0;
   return retval;
}

/******************************************************************************/
/*                             P o l l 2 E n u m                              */
/******************************************************************************/
  
int XrdSys::IOEvents::Poller::Poll2Enum(short events)
{
   if (events & POLLERR)  return EPIPE;

   if (events & POLLHUP)  return ECONNRESET;

   if (events & POLLNVAL) return EBADF;

   return EOPNOTSUPP;
}

/******************************************************************************/
/*                               S e n d C m d                                */
/******************************************************************************/
  
int XrdSys::IOEvents::Poller::SendCmd(PipeData &cmd)
{
   int wlen;

// Pipe writes are atomic so we don't need locks. Some commands require
// confirmation. We handle that here based on the command. Note that pipes
// gaurantee that all of the data will be written or we will block.
//
   if (cmd.req >= PipeData::Post)
      {XrdSysSemaphore mySem(0);
       cmd.theSem = &mySem;
       do {wlen = write(cmdFD, (char *)&cmd, sizeof(PipeData));}
          while (wlen < 0 && errno == EINTR);
       if (wlen > 0) mySem.Wait();
      } else {
       do {wlen = write(cmdFD, (char *)&cmd, sizeof(PipeData));}
          while (wlen < 0 && errno == EINTR);
      }

// All done
//
   return (wlen >= 0 ? 0 : errno);
}
  
/******************************************************************************/
/* Protected:                 S e t P o l l E n t                             */
/******************************************************************************/
  
void XrdSys::IOEvents::Poller::SetPollEnt(XrdSys::IOEvents::Channel *cP, int pe)
{
   cP->pollEnt = pe;
}

/******************************************************************************/
/*                                  S t o p                                   */
/******************************************************************************/

void XrdSys::IOEvents::Poller::Stop()
{
   PipeData  cmdbuff = {PipeData::Stop, 0};
   CallBack *theCB;
   Channel  *cP;
   void     *vrc, *cbArg;
   int       doCB;

// Lock all of this
//
   adMutex.Lock();

// If we are already shutdown then we are done
//
   if (cmdFD == -1) {adMutex.UnLock(); return;}

// First we must stop the poller thread in an orderly fashion.
//
   SendCmd(cmdbuff);

// Close the pipe communication mechanism
//
   close(cmdFD); cmdFD = -1;
   close(reqFD); reqFD = -1;

// Run through cleaning up the channels. While there should not be any other
// operations happening on this poller, we take the conservative approach.
//
   while((cP = attBase))
        {REMOVE(attBase, attList, cP);
         adMutex.UnLock();
         cP->chMutex.Lock();
         doCB = cP->chCB != 0 && (cP->chEvents & Channel::stopEvent);
         if (cP->inTOQ) TmoDel(cP);
         cP->Reset(&pollErr1, cP->chFD, EIDRM);
         cP->chPollXQ = &pollErr1;
         if (doCB)
            {cP->chStat = Channel::isClear;
             theCB = cP->chCB; cbArg = cP->chCBA;
             cP->chMutex.UnLock();
             theCB->Stop(cP, cbArg);
            } else cP->chMutex.UnLock();
         adMutex.Lock();
        }

// Now invoke the poller specific shutdown
//
   Shutdown();
   adMutex.UnLock();
}
  
/******************************************************************************/
/*                                T m o A d d                                 */
/******************************************************************************/
  
bool XrdSys::IOEvents::Poller::TmoAdd(XrdSys::IOEvents::Channel *cP)
{
   XrdSysMutexHelper mHelper(toMutex);
   time_t rdDL, wrDL;
   Channel *ncP;

// Remove element from timeout queue if it is there
//
   if (cP->inTOQ)
      {REMOVE(tmoBase, tmoList, cP);
       cP->inTOQ = 0;
      }

// Calculate the closest enabled deadline
//
   rdDL = (cP->chEvents & Channel:: readEvents ? cP->rdDL : maxTime);
   wrDL = (cP->chEvents & Channel::writeEvents ? cP->wrDL : maxTime);
   if (rdDL < wrDL) {cP->deadLine = rdDL; cP->dlType  = CallBack::ReadTimeOut;}
      else {if (rdDL != wrDL) cP->dlType = CallBack::WriteTimeOut;
               else cP->dlType  = CallBack::ReadTimeOut|CallBack::WriteTimeOut;
            cP->deadLine = wrDL;
           }

// If no timeout really applies, we are done
//
   if (cP->deadLine == maxTime) return false;

// Add the channel to the timeout queue in correct deadline position.
//
   if ((ncP = tmoBase))
      {do {if (cP->deadLine < ncP->deadLine) break;
           ncP = ncP->tmoList.next;
          } while(ncP != tmoBase);
       INSERT(tmoList, ncP, cP);
       if (cP->deadLine < tmoBase->deadLine) tmoBase = cP;
      } else tmoBase = cP;
   cP->inTOQ = 1;

// Indicate to the caller whether or not a wakeup is required
//
   return (tmoBase == cP);
}
  
/******************************************************************************/
/*                                T m o D e l                                 */
/******************************************************************************/

void XrdSys::IOEvents::Poller::TmoDel(XrdSys::IOEvents::Channel *cP)
{

// Get the timeout queue lock and remove the channel from the queue
//
   toMutex.Lock();
   REMOVE(tmoBase, tmoList, cP);
   cP->inTOQ = 0;
   toMutex.UnLock();
}

/******************************************************************************/
/*                                T m o G e t                                 */
/******************************************************************************/
  
int XrdSys::IOEvents::Poller::TmoGet()
{
   int wtval;

// Lock the timeout queue
//
   toMutex.Lock();

// Calculate wait time
//
   do {if (!tmoBase) {wtval = -1; break;}
       wtval = (tmoBase->deadLine - time(0)) * 1000;
       if (wtval > 0) break;
       CbkTMO();
      } while(1);

// Return the value
//
   wakePend = false;
   toMutex.UnLock();
   return wtval;
}

/******************************************************************************/
/*                                W a k e U p                                 */
/******************************************************************************/

void XrdSys::IOEvents::Poller::WakeUp()
{
   static PipeData cmdbuff = {PipeData::NoOp, 0};

// Send it off to wakeup the poller thread, but only if here is no wakeup in
// progress.
//
   toMutex.Lock();
   if (wakePend) toMutex.UnLock();
      else {wakePend = true;
            toMutex.UnLock();
            SendCmd(cmdbuff);
           }
}

/******************************************************************************/
/*              I m p l e m e n t a t i o n   S p e c i f i c s               */
/******************************************************************************/

#if defined( __solaris__ )  
#include "XrdSys/XrdSysIOEventsPollPort.icc"
#elif defined( __linux__ )
#include "XrdSys/XrdSysIOEventsPollE.icc"
#else
#include "XrdSys/XrdSysIOEventsPollPoll.icc"
#endif

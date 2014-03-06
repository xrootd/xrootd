#ifndef __XRDSYSIOEVENTS_HH__
#define __XRDSYSIOEVENTS_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S y s I O E v e n t s . h h                      */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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
  
#include <poll.h>
#include <time.h>
#include <sys/types.h>

#include "XrdSys/XrdSysPthread.hh"

//-----------------------------------------------------------------------------
//! IOEvents
//!
//! The classes here define a simple I/O event polling architecture suitable
//! for use with non-blocking devices. As it implements an event model, it is
//! not considered a high performance interface. For increased performance, you
//! need to use multiple polling event loops which effectively implements a
//! limited thread model for handling events. The implementation here is similar
//! to libEvent with better handling of timeouts and I/O polling resumption.
//!
//! While, channels are multi-thread safe, they cannot interlock with the state
//! of their file descriptor. You must first disable (via SetFD()) or delete
//! the channel before closing its associated file descriptor. This is the
//! only safe way to keep channels and their file descriptors synchronized.
//-----------------------------------------------------------------------------

namespace XrdSys
{
namespace IOEvents
{

/******************************************************************************/
/*                        C l a s s   C a l l B a c k                         */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! Interface
//!
//! The object used to effect a callback when an event occurs on a channel.
//! During the callback, all channels associated with the poller object doing
//! the callback are suspended until the callback object returns. All queued
//! callbacks are handled before the poller resumes any channels. This provides
//! simple serialization for all channels associated with a single poller.
//! You may call any channel method from a callback to effect appropriate
//! changes. You may also delete the channel at any time.
//-----------------------------------------------------------------------------
  
class Channel;
class CallBack
{
public:

//-----------------------------------------------------------------------------
//! Events that may cause a callback object to be activated.
//-----------------------------------------------------------------------------

      enum EventType
      {
        ReadyToRead  = 0x01,  //!< New data has arrived
        ReadTimeOut  = 0x02,  //!< Read timeout
        ReadyToWrite = 0x04,  //!< Writing won't block
        WriteTimeOut = 0x08,  //!< Write timeout
        ValidEvents  = 0x0f   //!< Mask to test for valid events
      };

//-----------------------------------------------------------------------------
//! Handle event notification. A method must be supplied. The enable/disable
//! status of the channel is not modified. To change the status, use the
//! channel's Enable() and Disable() method prior to returning. After return,
//! the current channel's status is used to determine how it will behave. If
//! the event is a timeout, the timeout becomes infinite for that event unless
//! Enable() is called for the event. This is to prevent timeout loops on
//! channels that remain enabled even after a timeout. Event loop callbacks
//! define a hazardous programming model. If you do not have a well defined
//! threading model, you should restrict yourself to dealing only with the
//! passed channel object in the callback so as to avoid deadlocks.
//!
//! @param  chP     the associated channel causing the callback.
//! @param  cbArg   the callback argument specified for the channel.
//! @param  evFlags events that caused this callback to be invoked. More than
//!                 one event may be indicated (see EventType above).
//!
//! @return true    Resume handling the channel with current status.
//!         false   Disable the channel and remove it from associated poller.
//-----------------------------------------------------------------------------

virtual bool Event(Channel *chP, void *cbArg, int evFlags) = 0;

//-----------------------------------------------------------------------------
//! Handle fatal error notification. This method is called only when error
//! events are specifically enabled (see Enable() for admonitions). It is
//! passed the reason for the error. Upon return, the channel is disabled but
//! stays attached to the poller so that it can be revitalized with SetFD().
//! You should replace this method if you specifically enable error events.
//!
//! @param  chP     the associated channel causing the callback.
//! @param  cbArg   the callback argument specified for the channel.
//! @param  eNum    the errno associated with the error.
//! @param  eTxt    descriptive name of the operation encountering the error.
//-----------------------------------------------------------------------------

virtual void Fatal(Channel *chP, void *cbArg, int eNum, const char *eTxt) {};

//-----------------------------------------------------------------------------
//! Handle poller stop notification. This method is called only when the poller
//! is stopped and the channel enabled the stop event. You should should replace
//! this method if you specifically enable the stop event. You must not invoke
//! channel methods in this callback, otherwise the results are unpredictable.
//!
//! @param  chP     the associated channel causing the callback.
//! @param  cbArg   the callback argument specified for the channel.
//-----------------------------------------------------------------------------

virtual void Stop(Channel *chP, void *cbArg) {}

//-----------------------------------------------------------------------------
//! Constructor
//-----------------------------------------------------------------------------

            CallBack() {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

virtual    ~CallBack() {}
};

/******************************************************************************/
/*                         C l a s s   C h a n n e l                          */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! Describe a channel that is capable of fielding events. A valid channel is
//! associated with a Poller object, a CallBack object, and an open file
//! descriptor. These are normally set at construction time.
//-----------------------------------------------------------------------------
  
class ChannelWait;
class Poller;
class Channel
{
friend class Poller;
public:

//-----------------------------------------------------------------------------
//! Delete a channel. You must use this method instead of delete. The Delete()
//! may block if an channel is being deleted outside of the poller thread.
//! When this object is deleted, all events are disabled, pending callbacks
//! are either completed or canceled, and the channel is removed from the
//! assigned poller. Only then is the storage freed.
//-----------------------------------------------------------------------------

        void Delete();

//-----------------------------------------------------------------------------
//! Event bits used to feed Enable() and Disable(); can be or'd.
//-----------------------------------------------------------------------------

enum EventCode {readEvents  = 0x01, //!< Read  and Read  Timeouts
                writeEvents = 0x04, //!< Write and Write Timeouts
                rwEvents    = 0x05, //!< Both of the above
                errorEvents = 0x10, //!< Error event non-r/w specific
                stopEvent   = 0x20, //!< Poller stop event
                allEvents   = 0x35  //!< All of the above
               };

//-----------------------------------------------------------------------------
//! Disable one or more events. Ignored for already disabled events.
//!
//! @param  events  one or more events or'd together (see EventCode above).
//! @param  eText   optional pointer to where an operation description is to be
//!                 placed when an error occurs (i.e. returns false).
//!
//! @return true    Events successfully disabled.
//!         false   Events not disabled; errno holds the error number and if
//!                 eText is supplied, points to the operation desscription.
//-----------------------------------------------------------------------------

        bool Disable(int events, const char **eText=0);

//-----------------------------------------------------------------------------
//! Enable one or more events. Events that are already enabled remain enabled
//! but may have their timeout value change.
//!
//! Enable can fail for many reasons. Most importantly, if the channel was
//! disabled for all events when a fatal error occurred; enabling it immediately
//! returns the fatal error without invoking the callback. This happens on
//! platforms that disallow physically masking out error events.
//!
//! Additionally, when an error occurs and the channel is not enabled for error
//! events but is enabled for read or write, the callback is called indicating
//! ReadyToRead or ReadyToWrite. A subsequent write will then end with an error
//! (typically, EPIPE) and a subsequent read will end with an erorr or indicate
//! zero bytes read; either of which should be treated as an error (typically,
//! POLLHUP). Generally, you should always allow separable error events.
//!
//! @param  events  one or more events or'd together (see EventCode above).
//! @param  timeout >0 maximum seconds that may elapsed before a timeout event
//!                    corresponding to the specified event(s) occurs.
//!                 =0 Keep whatever timeout is currently in effect from the
//!                    previous Enable() invocation for the event(s).
//!                 <0 No timeout applies.
//!                 There can be separate timeouts for read and write if
//!                 Enable() is separately called for each event code.
//!                 Otherwise, the timeout applies to all specified events.
//!                 The timeout is ignored for error events.
//! @param  eText   optional pointer to where an operation description is to be
//!                 placed when an error occurs (i.e. returns false).
//!
//! @return true    Events successfully enabled.
//!         false   Events not enabled; errno holds the error number and if
//!                 eText is supplied, points to the operation desscription.
//-----------------------------------------------------------------------------

        bool Enable(int events, int timeout=0, const char **eText=0);

//-----------------------------------------------------------------------------
//! Get the callback object and argument associated with this channel.
//!
//! @param cbP   Place where the pointer is to be returned.
//! @param caP   Place where the callback argument is to be returned.
//-----------------------------------------------------------------------------

        void GetCallBack(CallBack **cbP, void **cbArg);

//-----------------------------------------------------------------------------
//! Get the events that are currently enabled for this channel.
//!
//! @return >0      Event bits that are enabled (see EventCode above).
//!         =0      No events are enabled.
//!         <0      Channel not assigned to a Poller object.
//-----------------------------------------------------------------------------

inline  int  GetEvents() {return (chPoller ? static_cast<int>(chEvents) : -1);}

//-----------------------------------------------------------------------------
//! Get the file descriptor number associated with this channel.
//!
//! @return >=0     The file descriptor number.
//!         < 0     No file desciptor associated with the channel.
//-----------------------------------------------------------------------------

inline  int  GetFD() {return chFD;}

//-----------------------------------------------------------------------------
//! Set the callback object and argument associated with this channel.
//!
//! @param cbP   Pointer to the callback object (see above). The callback
//!              object must not be deleted while associated to a channel. A
//!              null callback object pointer effectively disables the channel.
//! @param cbArg The argument to be passed to the callback object.
//-----------------------------------------------------------------------------

        void SetCallBack(CallBack *cbP, void *cbArg=0);

//-----------------------------------------------------------------------------
//! Set a new file descriptor to be associated with this channel. The channel
//! is removed from polling consideration but remains attached to the poller.
//! The new file descriptor is recorded but the channel remains disabled. You
//! must use Enable() to add the file descriptor back to the polling set. This
//! allows you to retract a file descriptor about to be closed without having a
//! new file descriptor handy (e.g., use -1). This facilitates channel re-use.
//!
//! @param  fd   The associated file descriptor number.
//-----------------------------------------------------------------------------

        void SetFD(int fd);

//-----------------------------------------------------------------------------
//! Constructor.
//!
//! @param pollP Pointer to the poller object to which this channel will be
//!              assigned. Events are initially disabled after assignment and no
//!              timeout applies. Poller object assignment is permanent for the
//!              life of the channel object.
//! @param fd    The associated file descriptor number. It should not be
//!              assigned to any other channel and must be valid when the
//!              channel is enabled. Use SetFD() to set a new value.
//! @param cbP   Pointer to the callback object (see above). The callback
//!              object must not be deleted while associated to a channel.
//!              A callback object must exist in order for the channel to be
//!              enabled. Use SetCallBack() if you defered setting it here.
//! @param cbArg The argument to be passed to the callback object.
//-----------------------------------------------------------------------------

      Channel(Poller *pollP, int fd, CallBack *cbP=0, void *cbArg=0);

private:

//-----------------------------------------------------------------------------
//! Destuctor is private, use Delete() to delete this object.
//-----------------------------------------------------------------------------

     ~Channel() {}

struct dlQ {Channel *next; Channel *prev;};

XrdSysRecMutex chMutex;

dlQ            attList;     // List of attached channels
dlQ            tmoList;     // List of channels in the timeout queue

Poller        *chPoller;    // The effective poller
Poller        *chPollXQ;    // The real      poller
CallBack      *chCB;        // CallBack function
void          *chCBA;       // CallBack argument
int            chFD;        // Associated file descriptor
int            pollEnt;     // Used only for poll() type pollers
int            chRTO;       // Read  timeout value (0 means none)
int            chWTO;       // Write timeout value (0 means none)
time_t         rdDL;        // Read  deadline
time_t         wrDL;        // Write deadline
time_t         deadLine;    // The deadline in effect (read or write)
char           dlType;      // The deadline type in deadLine as CallBack type
char           chEvents;    // Enabled events as Channel type
char           chStat;      // Channel status below (!0 -> in callback mode)
enum Status   {isClear = 0, isCBMode, isDead};
char           inTOQ;       // True if the channel is in the timeout queue
char           inPSet;      // FD is in the actual poll set
char           reMod;       // Modify issued while defered, re-issue needed
short          chFault;     // Defered error, 0 if all is well

void           Reset(Poller *thePoller, int fd, int eNum=0);
};

/******************************************************************************/
/*                          C l a s s   P o l l e r                           */
/******************************************************************************/
  
//-----------------------------------------------------------------------------
//! Define a poller object interface. A poller fields and dispatches event
//! callbacks. An actual instance of a poller object is obtained by using the
//! Create() method. You cannot simply create an instance of this object using
//! new or in-place declaration since it is abstract. Any number of these
//! objects may created. Each creation spawns a polling thread.
//-----------------------------------------------------------------------------

class Poller
{
friend class BootStrap;
friend class Channel;
public:

//-----------------------------------------------------------------------------
//! Create a specialized instance of a poller object, initialize it, and start
//! the polling process. You must call Create() to obtain a specialized poller.
//!
//! @param  eNum   Place where errno is placed upon failure.
//! @param  eTxt   Place where a pointer to the description of the failing
//!                operation is to be set. If null, no description is returned.
//! @param  crOpts Poller options (see static const optxxx):
//!                optTOM   - Timeout resumption after a timeout event must be
//!                           manually reenabled. By default, event timeouts are
//!                           automatically renabled after successful callbacks.
//!
//! @return !0     Poller successfully created and started.
//!                eNum contains zero.
//!                eTxt if not null contains a null string.
//!                The returned value is a pointer to the Poller object.
//!          0     Poller could not be created.
//!                eNum contains the associated errno value.
//!                eTxt if not null contains the failing operation.
//-----------------------------------------------------------------------------

enum   CreateOpts  {optTOM};

static Poller     *Create(int &eNum, const char **eTxt=0, int crOpts=0);

//-----------------------------------------------------------------------------
//! Stop a poller object. Active callbacks are completed. Pending callbacks are
//! discarded. After which the poller event thread exits. Subsequently, each
//! associated channel is disabled and removed from the poller object. If the
//! channel is enabled for a StopEvent, the stop callback is invoked. However,
//! any attempt to use the channel methods that require an active poller will
//! return an error.
//!
//! Since a stopped poller cannot be restarted; the only thing left is to delete
//! it. This also applies to all the associated channels since they no longer
//! have an active poller.
//-----------------------------------------------------------------------------

       void        Stop();

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param cFD   The file descriptor to send commands to the poll thread.
//! @param rFD   The file descriptor to recv commands in the poll thread.
//-----------------------------------------------------------------------------

         Poller(int cFD, int rFD);

//-----------------------------------------------------------------------------
//! Destructor. Stop() is effecively called when this object is deleted.
//-----------------------------------------------------------------------------

virtual ~Poller() {}

protected:
struct  PipeData;

        void  CbkTMO();
        bool  CbkXeq(Channel *cP, int events, int eNum, const char *eTxt);
inline  int   GetFault(Channel *cP)   {return cP->chFault;}
inline  int   GetPollEnt(Channel *cP) {return cP->pollEnt;}
        int   GetRequest();
        bool  Init(Channel *cP, int &eNum, const char **eTxt, bool &isLockd);
inline  void  LockChannel(Channel *cP) {cP->chMutex.Lock();}
        int   Poll2Enum(short events);
        int   SendCmd(PipeData &cmd);
        void  SetPollEnt(Channel *cP, int ptEnt);
        bool  TmoAdd(Channel *cP, int tmoSet);
        void  TmoDel(Channel *cP);
        int   TmoGet();
inline  void  UnLockChannel(Channel *cP) {cP->chMutex.UnLock();}

//! Start the polling event loop. An implementation must be supplied. Begin()
//! is called via the internal BootStrap class from a new thread.
//!
virtual void Begin(XrdSysSemaphore *syncp, int &rc, const char **eTxt) = 0;

//! Remove a channel to the poll set. An implementation must be supplied.
//! The channel is locked when this method is called but must be unlocked by the
//! method if a command is sent to the poller thread and isLocked set to false.
//!
virtual void Exclude(Channel *cP, bool &isLocked, bool dover=1) = 0;

//! Add a channel to the poll set. An implementation must be supplied.
//! The channel is locked when this method is called but must be unlocked by the
//! method if a command is sent to the poller thread and isLocked set to false.
//!
virtual bool Include(Channel     *cP,
                     int         &eNum,
                     const char **eTxt,
                     bool        &isLocked) = 0;

//! Modify the event status of a channel. An implementation must be supplied.
//! The channel is locked when this method is called but must be unlocked by the
//! method if a command is sent to the poller thread and isLocked set to false.
//!
virtual bool Modify (Channel     *cP,
                     int         &eNum,
                     const char **eTxt,
                     bool        &isLocked) = 0;

//! Shutdown the poller. An implementation must be supplied. The shutdown method
//! must release any allocated storage and close private file descriptors. The
//! polling thread will have already been terminated and x-thread pipe closed.
//! Warning: the derived destructor *must* call Stop() and do nothing else!
//
virtual void Shutdown() = 0;

// The following is common to all implementations
//
Channel        *attBase;    // -> First channel in attach  queue or 0
Channel        *tmoBase;    // -> First channel in timeout queue or 0

pthread_t       pollTid;    // Poller's thread ID

struct pollfd   pipePoll;   // Stucture to wait for pipe events
int             cmdFD;      // FD to send PipeData commands
int             reqFD;      // FD to recv PipeData requests
struct          PipeData {char req; char evt; short ent; int fd;
                          XrdSysSemaphore *theSem;
                          enum cmd {NoOp = 0, MdFD = 1, Post = 2,
                                    MiFD = 3, RmFD = 4, Stop = 5};
                          PipeData(char reQ=0, char evT=0, short enT=0,
                                   int  fD =0, XrdSysSemaphore *sP=0)
                                  : req(reQ), evt(evT), ent(enT), fd(fD),
                                    theSem(sP) {}
                         ~PipeData() {}
                         };
PipeData        reqBuff;    // Buffer used by poller thread to recv data
char           *pipeBuff;   // Read resumption point in buffer
int             pipeBlen;   // Number of outstanding bytes
char            tmoMask;    // Timeout mask
bool            wakePend;   // Wakeup is effectively pending (don't send)
bool            chDead;     // True if channel deleted by callback

static time_t   maxTime;    // Maximum time allowed

private:

void Attach(Channel *cP);
void Detach(Channel *cP, bool &isLocked, bool keep=true);
void WakeUp();

// newPoller() called to get a specialized new poll object at in response to
//             Create(). A specialized implementation must be supplied.
//
static Poller *newPoller(int pFD[2], int &eNum, const char **eTxt);

XrdSysMutex    adMutex; // Mutex for adding & detaching channels
XrdSysMutex    toMutex; // Mutex for handling the timeout list
};
};
};
#endif

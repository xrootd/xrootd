#ifndef __XRDXROOTDMONITOR__
#define __XRDXROOTDMONITOR__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n i t o r . h h                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

#include <inttypes.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "XrdNet/XrdNetPeer.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XProtocol/XPtypes.hh"
  
/******************************************************************************/
/*                            X r d M o n i t o r                             */
/******************************************************************************/

#define XROOTD_MON_ALL      1
#define XROOTD_MON_NONE     0
#define XROOTD_MON_SOME    -1

class XrdScheduler;
  
class XrdXrootdMonitor
{
public:

// All values for Add_xx() must be passed in network byte order
//
inline void              Add_rd(kXR_int32 dictid, 
                                kXR_int32 rlen,
                                kXR_int64 offset)
                               {Add_io(dictid, rlen, offset);}

inline void              Add_wr(kXR_int32 dictid, 
                                kXR_int32 wlen, 
                                kXR_int64 offset)
                               {unsigned int temp = ~ntohl(wlen)+1;
                                Add_io(dictid,
                                      (kXR_int32)htonl(temp), offset);
                               }

static XrdXrootdMonitor *Alloc(int force=0);

inline void              Close(kXR_int32 dictid)
                              {do_OC(XROOTD_MON_CLOSE, dictid);}

       void              Flush();

static int               Init(XrdScheduler *sp, XrdOucError *errp,
                              char *dest, int msz=8192, int wsz=60);

       void              Map(const char code, kXR_int32 dictID,
                             const char *uname, const char *path);

inline void              Open(kXR_int32 dictid)
                             {do_OC(XROOTD_MON_OPEN, dictid);}

static void              setMode(int onoff);

static time_t            Tick();

                         XrdXrootdMonitor();
                        ~XrdXrootdMonitor(); 

private:

inline void              Add_io(kXR_int32 dictid, kXR_int32 buflen,
                                kXR_int64 offset);
inline void              do_OC(char opc, kXR_int32 dictid);
       void              fillHeader(XrdXrootdMonHeader *hdr,
                                    const char id, int size);
       void              Mark();
       int               Send(void *buff, int size);

static XrdScheduler      *Sched;
static XrdOucError       *eDest;
static XrdOucMutex        windowMutex;
static XrdNetPeer         monDest;
       XrdXrootdMonBuff  *monBuff;
static int                monBlen;
       int                nextEnt;
static int                lastEnt;
static kXR_int32          startTime;
       kXR_int32          lastWindow;
static kXR_int32          currWindow;
static kXR_int32          sizeWindow;
static int                isEnabled;
static int                numMonitor;
};

/******************************************************************************/
/*                      I n l i n e   F u n c t i o n s                       */
/******************************************************************************/
/******************************************************************************/
/*                                A d d _ i o                                 */
/******************************************************************************/
  
void XrdXrootdMonitor::Add_io(kXR_int32 dictid,kXR_int32 blen,kXR_int64 offset)
     {if (lastWindow != currWindow) Mark();
         else if (nextEnt == lastEnt) Flush();
      monBuff->info[nextEnt].offset.val    = offset;
      monBuff->info[nextEnt].arg1.buflen   = blen;
      monBuff->info[nextEnt++].arg2.dictid = dictid;
     }
  
/******************************************************************************/
/*                                 d o _ O C                                  */
/******************************************************************************/
  
void XrdXrootdMonitor::do_OC(char opc, kXR_int32 dictid)
     {if (lastWindow != currWindow) Mark();
         else if (nextEnt == lastEnt) Flush();
      monBuff->info[nextEnt].offset.id[0]  = opc;
      monBuff->info[nextEnt].arg1.buflen   = 0;
      monBuff->info[nextEnt++].arg2.dictid = dictid;
     }
#endif

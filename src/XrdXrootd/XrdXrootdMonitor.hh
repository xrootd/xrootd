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
#define XROOTD_MON_SOME     2
#define XROOTD_MON_FILE     4
#define XROOTD_MON_IO       8
#define XROOTD_MON_NONE     ~(XROOTD_MON_ALL | XROOTD_MON_SOME)

class XrdScheduler;
  
class XrdXrootdMonitor
{
public:

// All values for Add_xx() must be passed in network byte order
//
inline void              Add_rd(kXR_unt32 dictid,
                                kXR_int32 rlen,
                                kXR_int64 offset)
                               {Add_io(dictid, rlen, offset);}

inline void              Add_wr(kXR_unt32 dictid,
                                kXR_int32 wlen, 
                                kXR_int64 offset)
                               {unsigned int temp = ~ntohl(wlen)+1;
                                Add_io(dictid,
                                      (kXR_int32)htonl(temp), offset);
                               }

static XrdXrootdMonitor *Alloc(int force=0);

       void              Close(kXR_unt32 dictid, long long rTot, long long wTot);

       void              Flush();

static int               Init(XrdScheduler *sp, XrdOucError *errp,
                              char *dest, int msz=8192, int wsz=60);

       kXR_unt32         Map(const char code,const char *uname,const char *path);

inline void              Open(kXR_unt32 dictid);

static void              setMode(int onoff);

static time_t            Tick();

static char              monIO;

static char              monFILE;

                         XrdXrootdMonitor();
                        ~XrdXrootdMonitor(); 

private:

inline void              Add_io(kXR_unt32 dictid, kXR_int32 buflen,
                                kXR_int64 offset);
       unsigned char     do_Shift(long long xTot, unsigned int &xVal);
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
  
void XrdXrootdMonitor::Add_io(kXR_unt32 dictid,kXR_int32 blen,kXR_int64 offset)
     {if (lastWindow != currWindow) Mark();
         else if (nextEnt == lastEnt) Flush();
      monBuff->info[nextEnt].arg0.val      = offset;
      monBuff->info[nextEnt].arg1.buflen   = blen;
      monBuff->info[nextEnt++].arg2.dictid = dictid;
     }
  
/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
void XrdXrootdMonitor::Open(kXR_unt32 dictid)
     {if (lastWindow != currWindow) Mark();
         else if (nextEnt == lastEnt) Flush();
      monBuff->info[nextEnt].arg0.id[0]    = XROOTD_MON_OPEN;
      monBuff->info[nextEnt].arg1.buflen   = 0;
      monBuff->info[nextEnt++].arg2.dictid = dictid;
     }
#endif

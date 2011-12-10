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

#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "XrdNet/XrdNetPeer.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XProtocol/XPtypes.hh"

/******************************************************************************/
/*                            X r d M o n i t o r                             */
/******************************************************************************/

#define XROOTD_MON_ALL      1
#define XROOTD_MON_FILE     2
#define XROOTD_MON_IO       4
#define XROOTD_MON_INFO     8
#define XROOTD_MON_USER    16
#define XROOTD_MON_AUTH    32
#define XROOTD_MON_PATH    (XROOTD_MON_IO   | XROOTD_MON_FILE)
#define XROOTD_MON_REDR    64
#define XROOTD_MON_IOV    128

class XrdScheduler;
  
/******************************************************************************/
/*                C l a s s   X r d X r o o t d M o n i t o r                 */
/******************************************************************************/
  
class XrdXrootdMonitor
{
public:
       class User;
friend class User;

// All values for Add_xx() must be passed in network byte order
//
inline void              Add_rd(kXR_unt32 dictid,
                                kXR_int32 rlen,
                                kXR_int64 offset)
                               {Add_io(dictid, rlen, offset);}

inline void              Add_rv(kXR_unt32 dictid,
                                kXR_int32 rlen,
                                kXR_int16 vcnt,
                                kXR_char  vseq)
                               {if (lastWindow != currWindow) Mark();
                                   else if (nextEnt == lastEnt) Flush();
                                monBuff->info[nextEnt].arg0.id[0]    = XROOTD_MON_READV;
                                monBuff->info[nextEnt].arg0.id[1]    = vseq;
                                monBuff->info[nextEnt].arg0.sVal[1]  = vcnt;
                                monBuff->info[nextEnt].arg0.rTot[1]  = 0;
                                monBuff->info[nextEnt].arg1.buflen   = rlen;
                                monBuff->info[nextEnt++].arg2.dictid = dictid;
                               }

inline void              Add_wr(kXR_unt32 dictid,
                                kXR_int32 wlen, 
                                kXR_int64 offset)
                               {Add_io(dictid,(kXR_int32)htonl(-wlen),offset);}

       void              appID(char *id);

       void              Close(kXR_unt32 dictid, long long rTot, long long wTot);

       void              Disc(kXR_unt32 dictid, int csec, char Flags=0);

static void              Defaults(char *dest1, int m1, char *dest2, int m2);
static void              Defaults(int msz, int rsz, int wsz,
                                  int flush, int flash, int iDent);

static void              Ident() {Send(-1, idRec, idLen);}

static int               Init(XrdScheduler *sp,    XrdSysError *errp,
                              const char   *iHost, const char  *iProg,
                              const char   *iName, int Port);

       void              Open(kXR_unt32 dictid, off_t fsize);

static time_t            Tick();

class  User
{
public:

XrdXrootdMonitor *Agent;
kXR_unt32         Did;
char              Iops;
char              Fops;
short             Len;
char             *Name;

inline int         Auths() {return XrdXrootdMonitor::monAUTH;}

void               Clear() {if (Name)  {free(Name); Name = 0; Len = 0;}
                            if (Agent) {Agent->unAlloc(Agent); Agent = 0;}
                            Did = 0; Iops = Fops = 0;
                           }

       void        Enable();

       void        Disable();

inline int         Files()  {return (Agent ? Fops : 0);}

inline int         Info()   {return (Agent ? XrdXrootdMonitor::monINFO : 0);}

inline int         InOut()  {return (Agent ? Iops : 0);}

inline int         Logins() {return (Agent ? XrdXrootdMonitor::monUSER : 0);}

inline kXR_unt32   MapInfo(const char *Info)
                          {return XrdXrootdMonitor::Map(XROOTD_MON_MAPINFO,
                                                        *this, Info);
                          }

inline kXR_unt32   MapPath(const char *Path)
                          {return XrdXrootdMonitor::Map(XROOTD_MON_MAPPATH,
                                                        *this, Path);
                          }

       void        Register(const char *Uname, const char *Hname);

       void        Report(const char *Info)
                         {Did=XrdXrootdMonitor::Map(XROOTD_MON_MAPUSER,*this,Info);}

inline int         Ready()  {return XrdXrootdMonitor::monACTIVE;}

       User() : Agent(0), Did(0), Iops(0), Fops(0), Len(0), Name(0) {}
      ~User() {Clear();}
};

static XrdXrootdMonitor *altMon;

                         XrdXrootdMonitor();

private:
                        ~XrdXrootdMonitor(); 

inline void              Add_io(kXR_unt32 duid, kXR_int32 blen, kXR_int64 offs)
                               {if (lastWindow != currWindow) Mark();
                                   else if (nextEnt == lastEnt) Flush();
                                monBuff->info[nextEnt].arg0.val      = offs;
                                monBuff->info[nextEnt].arg1.buflen   = blen;
                                monBuff->info[nextEnt++].arg2.dictid = duid;
                               }
static XrdXrootdMonitor *Alloc(int force=0);
       unsigned char     do_Shift(long long xTot, unsigned int &xVal);
       void              Dup(XrdXrootdMonTrace *mrec);
static void              fillHeader(XrdXrootdMonHeader *hdr,
                                    const char id, int size);
       void              Flush();
static kXR_unt32         Map(char  code, XrdXrootdMonitor::User &uInfo,
                             const char *path);
       void              Mark();
static int               Send(int mmode, void *buff, int size);
static void              startClock();
static void              unAlloc(XrdXrootdMonitor *monp);

static XrdScheduler      *Sched;
static XrdSysError       *eDest;
static XrdSysMutex        windowMutex;
static char              *idRec;
static int                idLen;
static int                monFD;
static char              *Dest1;
static int                monMode1;
static struct sockaddr    InetAddr1;
static char              *Dest2;
static int                monMode2;
static struct sockaddr    InetAddr2;
       XrdXrootdMonBuff  *monBuff;
static int                monBlen;
       int                nextEnt;
static int                lastEnt;
static int                autoFlash;
static int                autoFlush;
static int                FlushTime;
static kXR_int32          startTime;
       kXR_int32          lastWindow;
static kXR_int32          currWindow;
static kXR_int32          sizeWindow;
static int                isEnabled;
static int                numMonitor;
static int                monIdent;
static int                monRlen;
static long long          mySID;
static char               sidName[16];
static short              sidSize;
static char               monIO;
static char               monINFO;
static char               monFILE;
static char               monMIGR;
static char               monPURGE;
static char               monREDR;
static char               monSTAGE;
static char               monUSER;
static char               monAUTH;
static char               monACTIVE;
};
#endif

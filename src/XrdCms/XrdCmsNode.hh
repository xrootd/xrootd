#ifndef __CMS_NODE__H
#define __CMS_NODE__H
/******************************************************************************/
/*                                                                            */
/*                         X r d C m s N o d e . h h                          */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/uio.h>
  
#include "Xrd/XrdLink.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdCms/XrdCmsRRQ.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysRAtomic.hh"

class XrdCmsBaseFR;
class XrdCmsBaseFS;
class XrdCmsClustID;
class XrdCmsDrop;
class XrdCmsManager;
class XrdCmsPrepArgs;
class XrdCmsRRData;
class XrdCmsSelect;
class XrdCmsSelected;
class XrdOucProg;

class XrdCmsNode
{
friend class XrdCmsCluster;
public:
       char  *Ident     = 0; // -> role hostname
       char   hasNet    = 0; //0 Network selection mask
       char   isBad     = 0; //1 Set on an event that makes it unselectable
       char   isOffline;     //2 Set when a link failure occurs (constructor)
       char   isRW      = 0; //3 Set when node can write or stage data
       char   isNoStage = 0; //4 Set upon a nostage event
       char   isMan     = 0; //5 Set when node acts as manager
       char   isPeer    = 0; //6 Set when node acts as peer manager
       char   isBound   = 0; //7 Set when node is in the configuration
       char   isKnown   = 0; //0 Set when we have recieved a "state"
       char   isConn    = 0; //1 Set when node is network connected
       char   isGone    = 0; //2 Set when node must be deleted
       char   isPerm    = 0; //3 Set when node is permanently bound
       char   rsvd      = 0; //4 Reserved
       char   RoleID    = 0; //5 The converted XrdCmsRole::RoleID
       char   TimeZone  = 0; //6 Time zone in +UTC-
       char   TZValid   = 0; //7 Time zone has been set

static const char isBlisted  = 0x01; // in isBad -> Node is black listed
static const char isDisabled = 0x02; // in isBad -> Node is disable (internal)
static const char isSuspend  = 0x04; // in isBad -> Node is suspended via event
static const char isDoomed   = 0x08; // in isBad -> Node socket must be closed

static const char allowsRW   = 0x01; // in isRW  -> Server allows r/w access
static const char allowsSS   = 0x02; // in isRW  -> Server can stage data

unsigned int    DiskTotal = 0;// Total disk space in GB
         int    DiskNums  = 0;// Number of file systems
         int    DiskMinF  = 0;// Minimum MB needed for selection
         int    DiskFree  = 0;// Largest free MB
         int    DiskUtil  = 0;// Total disk utilization
unsigned int    ConfigID  = 0;// Configuration identifier

const  char  *do_Avail(XrdCmsRRData &Arg);
const  char  *do_Chmod(XrdCmsRRData &Arg);
const  char  *do_Disc(XrdCmsRRData &Arg);
const  char  *do_Gone(XrdCmsRRData &Arg);
const  char  *do_Have(XrdCmsRRData &Arg);
const  char  *do_Load(XrdCmsRRData &Arg);
const  char  *do_Locate(XrdCmsRRData &Arg);
static int    do_LocFmt(char *buff, XrdCmsSelected *sP,
                        SMask_t pf, SMask_t wf,
                        bool lsall=false, bool lsuniq=false);
const  char  *do_Mkdir(XrdCmsRRData &Arg);
const  char  *do_Mkpath(XrdCmsRRData &Arg);
const  char  *do_Mv(XrdCmsRRData &Arg);
const  char  *do_Ping(XrdCmsRRData &Arg);
const  char  *do_Pong(XrdCmsRRData &Arg);
const  char  *do_PrepAdd(XrdCmsRRData &Arg);
const  char  *do_PrepDel(XrdCmsRRData &Arg);
const  char  *do_Rm(XrdCmsRRData &Arg);
const  char  *do_Rmdir(XrdCmsRRData &Arg);
       int    do_SelAvoid(XrdCmsRRData &Arg, XrdCmsSelect &Sel,
                          char *Avoid, bool &doRedir);
const  char  *do_Select(XrdCmsRRData &Arg);
static int    do_SelPrep(XrdCmsPrepArgs &Arg);
const  char  *do_Space(XrdCmsRRData &Arg);
const  char  *do_State(XrdCmsRRData &Arg);
static void   do_StateDFS(XrdCmsBaseFR *rP, int rc);
       int    do_StateFWD(XrdCmsRRData &Arg);
const  char  *do_StatFS(XrdCmsRRData &Arg);
const  char  *do_Stats(XrdCmsRRData &Arg);
const  char  *do_Status(XrdCmsRRData &Arg);
const  char  *do_Trunc(XrdCmsRRData &Arg);
const  char  *do_Try(XrdCmsRRData &Arg);
const  char  *do_Update(XrdCmsRRData &Arg);
const  char  *do_Usage(XrdCmsRRData &Arg);

       void   Delete(XrdSysRWLock &gMutex)
                    {XrdSysFusedMutex gMeld(gMutex); Delete(gMeld);}

       void   Delete(XrdSysMutex &gMutex)
                    {XrdSysFusedMutex gMeld(gMutex); Delete(gMeld);}

       void   Delete(XrdSysFusedMutex &gMutex);

       void   Disc(const char *reason=0, int needLock=1);

inline int    ID(int &INum) {INum = Instance; return NodeID;}

inline int    Inst() {return Instance;}

       bool   inDomain() {return netIF.InDomain(&netID);}

inline int    isNode(SMask_t smask) {return (smask & NodeMask) != 0;}

inline int    isNode(const XrdNetAddr *addr) // Only for avoid processing!
                    {return netID.Same(addr);}

inline int    isNode(XrdLink *lp, const char *nid, int port)
                    {if (nid)
                        {if (strcmp(myNID, nid)) return 0;
                         if (*nid == '*') return 1;
                        }
                     return netID.Same(lp->NetAddr()) && port == netIF.Port();
                    }

inline char  *Name()   {return (myName ? myName : (char *)"?");}

inline SMask_t Mask() {return NodeMask;}

inline void    g2nLock(XrdSysRWLock &gMutex)
                      {refCnt++;         // Keep node alive during transition
                       gMutex.UnLock();  // The lock must have ben held
                       nodeMutex.Lock(); // Downgrade to node lock
                      }

inline void    n2gLock(XrdSysRWLock &gMutex, bool rdlock=false)
                      {nodeMutex.UnLock(); // Release this node
                       refCnt--;           // OK for node to go away
                       if (rdlock) gMutex.ReadLock();
                          else     gMutex.WriteLock();
                      }

inline void    Lock() {refCnt++; nodeMutex.Lock();}

inline void  UnLock() {nodeMutex.UnLock(); refCnt--;}

inline void    Ref() {refCnt++;} // Must have global or node locked!
inline void  unRef() {refCnt--;}

static void  Report_Usage(XrdLink *lp);

inline int   Send(const char *buff, int blen=0)
                 {return (isOffline ? -1 : Link->Send(buff, blen));}
inline int   Send(const struct iovec *iov, int iovcnt, int iotot=0)
                 {return (isOffline ? -1 : Link->Send(iov, iovcnt, iotot));}

       void  setManager(XrdCmsManager *mP) {Manager = mP;}

       void  setName(XrdLink *lnkp, const char *theIF, int port);

       void  setShare(int shrval)
                     {if (shrval > 99) Shrem = Shrip = Share = 0;
                         else {Shrem = Share = shrval; Shrip = 100 - shrval;}
                     }

       int   setTZone(int tZone)
                     {TimeZone = tZone & 0x0f;
                      if (tZone & 0x10) TimeZone = -TimeZone;
                      TZValid = (tZone != 0);
                      return TimeZone;
                     }

        void setVersion(unsigned short vnum) {myVersion = vnum;}

inline void  setSlot(short rslot) {RSlot = rslot;}
inline short getSlot() {return RSlot;}

inline void  ShowIF() {netIF.Display("=====> ");}

       void  SyncSpace();

             XrdCmsNode(XrdLink *lnkp, const char *theIF=0, const char *sid=0,
                        int port=0, int lvl=0, int id=-1);
            ~XrdCmsNode();

private:
static const int fsL2PFail1 = 999991;
static const int fsL2PFail2 = 999992;

       void  DeleteWarn(unsigned int lkVal);
       int   fsExec(XrdOucProg *Prog, char *Arg1, char *Arg2=0);
const  char *fsFail(const char *Who, const char *What, const char *Path, int rc);
       int   getMode(const char *theMode, mode_t &Mode);
       int   getSize(const char *theSize, long long &Size);
       void  setHash(XrdCmsSelect &Sel, int acount);

XrdSysMutex        nodeMutex;
RAtomic_uint       refCnt{0};    // Tracks references to this onnject

XrdLink           *Link;         // Constructor
XrdNetAddr         netID;        // Constructor
XrdNetIF           netIF;        // Constructor
XrdCmsManager     *Manager  = 0;
time_t             DropTime = 0;
XrdCmsDrop        *DropJob  = 0;

XrdCmsClustID     *cidP     = 0;
SMask_t            NodeMask;
int                NodeID;
int                Instance;
int                myLevel;
short              subsPort = 0; // Subscription port number
unsigned short     myVersion;    // Constructor
char              *myCID;        // Constructor
char              *myNID;        // Constructor
char              *myName   = 0;
int                myNlen   = 0;

int                logload;
int                myCost   = 0; // Overall cost (determined by location)
int                myLoad   = 0; // Overall load
int                myMass   = 0; // Overall load including space utilization
RAtomic_int        RefW{0};      // Number of times used for writing
RAtomic_int        RefTotW{0};   // Actual total w/o share adjustments
RAtomic_int        RefR{0};      // Number of times used for redirection
RAtomic_int        RefTotR{0};   // Actual total w/o share adjustments
short              RSlot    = 0;
char               Share    = 0; // Share of requests for this node (0 -> n/a)
RAtomic_char       Shrem{0};     // Share of requests left
RAtomic_char       Shrin{0};     // Share intervals used
char               Shrip    = 0; // Share of requests to skip (set once)
char               Rsvd[3];

// The following fields are used to keep the supervisor's free space value
//
static XrdSysMutex mlMutex;
static int         LastFree;
};
#endif

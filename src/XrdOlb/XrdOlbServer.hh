#ifndef __OLB_SERVER__H
#define __OLB_SERVER__H
/******************************************************************************/
/*                                                                            */
/*                       X r d O l b S e r v e r . h h                        */
/*                                                                            */
/* (c) 2002 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <unistd.h>
  
#include "XrdOlb/XrdOlbTypes.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucLink.hh"
#include "XrdOuc/XrdOucPthread.hh"

class XrdOlbDrop;
class XrdOlbPrepArgs;

class XrdOlbServer
{
public:
friend class XrdOlbManager;

       char   isDisable;
       char   isOffline;
       char   isNoStage;
       char   isSuspend;
       char   isActive;
       char   isBound;

inline int   isServer(SMask_t smask) {return (smask & ServMask) != 0;}
inline int   isServer(char *hn) 
                      {return Link && !strcmp(Link->Name(), hn);}
inline int   isServer(unsigned long ipa)
                      {return ipa == IPAddr;}
inline char *Name()   {return (myName ? myName : (char *)"?");}
inline void    Lock() {myMutex.Lock();}
inline void  UnLock() {myMutex.UnLock();}

       int  Login(int Port, int suspended, int nostaging);

       void Process_Director(void);
       int  Process_Requests(int onlyone=0);
       int  Process_Responses(int onlyone=0);

static int  Resume(XrdOlbPrepArgs *pargs);

       int  Send(char *buff, int blen=0);
       int  Send(const struct iovec *iov, int iovcnt);

       void setName(char *hname, int port);

            XrdOlbServer(XrdOucLink *lnkp, int port=0);
           ~XrdOlbServer();

private:
       int   do_AvKb(char *rid);
       int   do_Chmod(char *rid, int do4real);
       int   do_Delay(char *rid);
       int   do_Gone(char *rid);
       int   do_Have(char *rid);
       int   do_Load(char *rid);
       int   do_Mkdir(char *rid, int do4real);
       int   do_Mv(char *rid, int do4real);
       int   do_Ping(char *rid);
       int   do_Pong(char *rid);
       int   do_Port(char *rid);
       int   do_PrepAdd(char *rid, int server=0);
       int   do_PrepAdd4Real(XrdOlbPrepArgs &pargs);
       int   do_PrepDel(char *rid, int server=0);
static int   do_PrepSel(XrdOlbPrepArgs *pargs, int stage);
       int   do_Rm(char *rid, int do4real);
       int   do_Rmdir(char *rid, int do4real);
       int   do_Select(char *rid, int reset=0);
       int   do_Space(char *rid);
       int   do_State(char *rid, int mustresp);
       int   do_Stats(char *rid, int wantdata);
       int   do_StNst(char *rid, int Resume);
       int   do_SuRes(char *rid, int Resume);
       int   do_Usage(char *rid);
static int   Inform(const char *cmd, XrdOlbPrepArgs *pargs);
       int   isOnline(char *path, int upt=1);
       char *Receive(char *idbuff, int blen);
       int   Reissue(char *rid, const char *op, char *arg1, char *path, char *arg3=0);

XrdOucHash<char> *PendPaths;
XrdOucMutex       myMutex;
XrdOucLink       *Link;
unsigned long     IPAddr;
XrdOlbServer     *Next;
time_t            DropTime;
XrdOlbDrop       *DropJob;

SMask_t    ServMask;
int        ServID;
int        Instance;
int        Port;
char      *myName;

int        pingpong;     // Keep alive field
int        newload;
int        logload;
int        DiskFree;     // Largest free KB
int        DiskNums;     // Number of file systems
int        DiskTota;     // Total free KB across all file systems
int        DiskAskdl;    // Deadline for asking about disk usage
int        myLoad;       // Overall load
int        RefA;         // Number of times used for allocation
int        RefTotA;
int        RefR;         // Number of times used for redirection
int        RefTotR;
};
#endif

#ifndef __OLB_MANAGER__H
#define __OLB_MANAGER__H
/******************************************************************************/
/*                                                                            */
/*                      X r d O l b M a n a g e r . h h                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/uio.h>
  
#include "XrdOlb/XrdOlbTypes.hh"
#include "XrdOuc/XrdOucPthread.hh"

class XrdOlbServer;
class XrdOucLink;

// Options for ListServers
//
#define OLB_LS_BEST   0x0001
#define OLB_LS_ALL    0x0002

/******************************************************************************/
/*                            o o l b _ S I n f o                             */
/******************************************************************************/
  
class XrdOlbSInfo
{
public:

XrdOlbSInfo *next;
char        *Name;
int          Id;
int          Load;
int          Free;
int          RefTotA;
int          RefTotR;
int          Status;

            XrdOlbSInfo(char *sname, XrdOlbSInfo *np=0)
                      {Name = (sname ? strdup(sname) : 0); next=np;}

           ~XrdOlbSInfo() {if (Name) free(Name);}
};
 
/******************************************************************************/
/*                          o o l b _ M a n a g e r                           */
/******************************************************************************/
  
// This a single-instance global class
//
class XrdOlbManager
{
public:

int         ServCnt;

void        Broadcast(SMask_t smask, char *buff, int blen);
void        Broadcast(SMask_t smask, const struct iovec *, int iovcnt);
void        Inform(const char *cmd, int clen=0, char *arg=0, int alen=0);
XrdOlbSInfo *ListServers(SMask_t mask=(SMask_t)-1, int opts=0);
void       *Login(XrdOucLink *lnkp);
void       *MonPerf(void);
void       *MonRefs(void);
void       *Pander(char *master, int port);
void       *Process(void);
void        Remove_Server(const char *reason, int sent, int sinst);
void        ResetRef(SMask_t smask);
void       *Respond(void);
void        Resume();
int         SelServer(int pt, char *path, SMask_t pmsk, SMask_t amsk, char *hb,
                      const struct iovec *iodata=0, int iovcnt=0);
void        setPort(int port) {Port = port;}
int         Snooze(int slpsec);
int         Stats(char *bfr, int bln);
void       *StartUDP(int formaster);
void        Suspend();

      XrdOlbManager();
     ~XrdOlbManager() {} // This object should never be deleted

private:
SMask_t      AddPath(XrdOlbServer *sp);
int          Add_Master(XrdOlbServer *sp);
XrdOlbServer *AddServer(XrdOucLink *lp, int port);
void        *Login_Failed(const char *reason, XrdOucLink *lp, XrdOlbServer *sp=0);
void         Record(char *path, const char *reason);
void         Remove_Master(const char *reason, XrdOlbServer *sp);
XrdOlbServer *SelbyLoad(SMask_t mask, int &nump, int &numf, int needspace);
XrdOlbServer *SelbyRef(SMask_t mask,  int &nump, int &numf, int needspace);

XrdOucMutex   STMutex;
XrdOlbServer *ServTab[XrdOlbSTMAX];
XrdOlbServer *ServBat[XrdOlbSTMAX];
XrdOlbServer *MastTab[XrdOlbSTMAX];

int  MTHi;
int  STHi;
int  InstNum;
int  XWait;
int  Port;
};
#endif

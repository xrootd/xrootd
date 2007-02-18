#ifndef _CS2_DCM_H_
#define _CS2_DCM_H_
/******************************************************************************/
/*                                                                            */
/*                          X r d C S 2 D C M . h h                           */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPthread.hh"
  
class XrdNetLink;
class XrdNetSocket;

class XrdCS2DCM
{
public:

void  Cleanup();
void  Cleanup(const char *);

int   Configure(int argc, char **argv);

void  doEvents();

void  doRequests();

void  Event(const char *Tid, const char *ReqID, const char *Mode, const char *Lfn);

void  Stage(const char *, char *, char *, char *, char *);

      XrdCS2DCM();
     ~XrdCS2DCM() {}

private:

int   CS2_Open(const char *Tid, const char *Fid, char *Lfn,
               int flags, off_t fsize);
int   CS2_rDone(const char *, unsigned long long, const char *);
int   CS2_wDone(const char *, unsigned long long, const char *);
int   CS2_wFail(const char *, unsigned long long, const char *, int);
int   CS2_Init();
void  failRequest(char *Pfn);
int   makeFname(char *, const char *, int, const char *);
int   makePath(char *fn);
void  Prep(const char *, const char *);
int   Release(const char *, const char *, int failed=0);
void  rmStale(const char *, time_t Deadline);
int   Setup();
void  LockDir()  {dirMutex.Lock();}
void  UnLockDir(){dirMutex.UnLock();}

XrdOucMutex     dirMutex;
XrdOucStream    Request;
XrdOucStream    Events;
XrdNetLink     *olbdLink;
char           *APath;   // Active
int             APlen;
char           *CPath;   // Closed
int             CPlen;
char           *EPath;   // Event FIFO path
int             EPlen;
char           *MPath;   // Management path
int             MPlen;
char           *PPath;   // Pending
int             PPlen;
char           *XPath;   // Base path that the above derive from
int             XPlen;
pid_t           Parent;
int             QLim;
time_t          UpTime;
};
#endif

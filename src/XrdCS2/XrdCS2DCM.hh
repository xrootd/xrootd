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
  
class XrdNetSocket;

class XrdCS2DCM
{
public:

int   Configure(int argc, char **argv);

void  doEvents();

void  doRequests();

void  Event(const char *Tid, char *ReqID, char *Mode, char *Lfn);

void  Stage(const char *, char *, char *, char *, char *);

      XrdCS2DCM();
     ~XrdCS2DCM() {}

private:

int   CS2_Open(const char *Tid, const char *Fid, char *Lfn,
               int flags, off_t fsize);
int   CS2_rDone(const char *Tid, unsigned long long reqID, char *Lfn);
int   CS2_wDone(const char *Tid, unsigned long long reqID, char *Pfn);
int   CS2_Init();
void  failRequest(char *Pfn);
int   makeFname(char *thePath, const char *fn);
int   makePath(char *fn);
void  Release(const char *, char *, char *);
int   Setup();
void  LockDir()  {dirMutex.Lock();}
void  UnLockDir(){dirMutex.UnLock();}

XrdOucMutex     dirMutex;
XrdOucStream    Request;
XrdOucStream    Events;
char           *MPath;
int             MPlen;
};
#endif

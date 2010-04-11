#ifndef __FRMXFRQUEUE_H__
#define __FRMXFRQUEUE_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d F r m X f r Q u e u e . h h                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"

class  XrdFrmReqFile;
class  XrdFrmRequest;
class  XrdFrmXfrJob;

class XrdFrmXfrQueue
{
public:

static int           Add(XrdFrmRequest *rP, XrdFrmReqFile *reqF, int theQ);

static void          Done(XrdFrmXfrJob *xP, const char *Msg);

static XrdFrmXfrJob *Get();

static int           Init();

static void          StopMon(void *parg);

static const int     stgQ = 0;  // Stage    queue
static const int     migQ = 1;  // Migrate  queue
static const int     getQ = 2;  // Copy in  queue
static const int     putQ = 3;  // Copy out queue
static const int     nilQ = 4;  // Empty    queue
static const int     numQ = 5;
static const int     outQ = 1;  // Used as a mask only

                     XrdFrmXfrQueue() {}
                    ~XrdFrmXfrQueue() {}

private:

static XrdFrmXfrJob *Pull();
static int           Notify(XrdFrmRequest *rP, int rc, const char *msg=0);
static void          Send2File(char *Dest, char *Msg, int Mln);
static void          Send2UDP(char *Dest, char *Msg, int Mln);
static int           Stopped(int qNum);
static const char   *xfrName(XrdFrmRequest &reqData, int isOut);

static XrdSysMutex               hMutex;
static XrdOucHash<XrdFrmXfrJob>  hTab;

static XrdSysMutex               qMutex;
static XrdSysSemaphore           qReady;

struct theQueue
      {XrdSysSemaphore           Avail;
       struct XrdFrmXfrJob      *Free;
       struct XrdFrmXfrJob      *First;
       struct XrdFrmXfrJob      *Last;
              XrdSysSemaphore    Alert;
              const char        *File;
              const char        *Name;
              int                Stop;
              int                qNum;
              theQueue() : Avail(0),Free(0),First(0),Last(0),Alert(0),Stop(0) {}
             ~theQueue() {}
      };
static theQueue                  xfrQ[numQ];
};
#endif

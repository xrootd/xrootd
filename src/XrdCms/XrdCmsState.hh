#ifndef __XRDCMSSTATE_H_
#define __XRDCMSSTATE_H_
/******************************************************************************/
/*                                                                            */
/*                        X r d C m s S t a t e . h h                         */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCms/XrdCmsTypes.hh"

class XrdLink;

class XrdCmsState
{
public:

int   Suspended;

void  Calc(int add2Stage, int add2Active);

void  Enable(char *theState=0);

void *Monitor();

void  sendState(XrdLink *Link);

void  setNodeCnt(int val) {myMutex.Lock(); minNodeCnt=val; myMutex.UnLock();}

      XrdCmsState();
     ~XrdCmsState() {}

private:
  
static const int All_Suspend = 1;
static const int All_NoStage = 2;

XrdSysSemaphore mySemaphore;
XrdSysMutex     myMutex;

int             minNodeCnt;
int             numActive;
int             numStaging;
int             curState;
int             Changes;
int             Enabled;
int             Disabled;
};

namespace XrdCms
{
extern    XrdCmsState CmsState;
}
#endif

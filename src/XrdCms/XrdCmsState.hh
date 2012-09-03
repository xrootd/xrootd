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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCms/XrdCmsTypes.hh"

class XrdLink;

class XrdCmsState
{
public:

int   Suspended;
int   NoStaging;

void  Enable();

void *Monitor();

int   Port();

void  sendState(XrdLink *Link);

void  Set(int ncount);
void  Set(int ncount, int isman, const char *AdminPath);

enum  StateType {Active = 0, Counts, FrontEnd, Space, Stage};

void  Update(StateType StateT, int ActivVal, int StageVal=0);

      XrdCmsState();
     ~XrdCmsState() {}
  
static const char SRV_Suspend = 1;
static const char FES_Suspend = 2;
static const char All_Suspend = 3;
static const char All_NoStage = 4;

private:
unsigned char Status(int Changes, int theState);

XrdSysSemaphore mySemaphore;
XrdSysMutex     myMutex;

const char     *NoStageFile;
const char     *SuspendFile;

int             minNodeCnt;   // Minimum number of needed subscribers
int             numActive;    // Number of active subscribers
int             numStaging;   // Number of subscribers that can stage
int             dataPort;     // Current data port number

char            currState;    // Current  state
char            prevState;    // Previous state
char            feOK;         // Front end functioning
char            noSpace;      // We don't have enough space
char            adminSuspend; // Admin asked for suspension
char            adminNoStage; // Admin asked for no staging
char            isMan;        // We are a manager (i.e., have redirectors)
char            Enabled;      // We are now enabled for reporting
};

namespace XrdCms
{
extern    XrdCmsState CmsState;
}
#endif

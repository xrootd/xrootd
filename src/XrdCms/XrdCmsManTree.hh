#ifndef __XRDCMSMANTREE_HH_
#define __XRDCMSMANTREE_HH_
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s M a n T r e e . h h                       */
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

#include "XrdCms/XrdCmsManager.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdCmsNode;
  
class XrdCmsManTree
{
public:

void Abort();

int  Connect(int nID, XrdCmsNode *nP);

void Disc(int nID);

int  Register();

int  Trying(int nID, int Lvl);

enum connStat {Aborted, Active, Connected, None, Pending, Waiting};

     XrdCmsManTree(int maxC);
    ~XrdCmsManTree() {};

private:

void Redrive(int nID) {tmInfo[nID].Status = Active;
                       tmInfo[nID].theSem.Post();
                       numWaiting--;
                      }
void Pause(int nID)   {tmInfo[nID].Status = Waiting;
                       numWaiting++;
                       myMutex.UnLock();
                       tmInfo[nID].theSem.Wait();
                      }

XrdSysMutex     myMutex;


struct TreeInfo
       {XrdSysSemaphore theSem;
        XrdCmsNode     *nodeP;
        connStat        Status;
        int             Level;

        TreeInfo() : theSem(0), nodeP(0), Status(None), Level(0) {};
       ~TreeInfo() {};

       }         tmInfo[XrdCmsManager::MTMax];

char            buff[16];
int             maxTMI;
int             numConn;
int             maxConn;
int             atRoot;
int             conLevel;
int             conNID;
int             numWaiting;
connStat        myStatus;
};
#endif

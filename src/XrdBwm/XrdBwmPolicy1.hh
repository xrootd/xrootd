#ifndef __BWM_POLICY1_HH__
#define __BWM_POLICY1_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d B w m P o l i c y 1 . h h                       */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdBwm/XrdBwmPolicy.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdBwmPolicy1 : public XrdBwmPolicy
{
public:

int  Dispatch(char *RespBuff, int RespSize);

int  Done(int rHandle);

int  Schedule(char *RespBuff, int RespSize, SchedParms &Parms);

void Status(int &numqIn, int &numqOut, int &numXeq);

     XrdBwmPolicy1(int inslots, int outslots);
    ~XrdBwmPolicy1() {}

enum Flow {In = 0, Out = 1, Xeq = 2, IOX = 3};

struct refReq
      {refReq *Next;
       int     refID;
       Flow    Way;

       refReq(int id, XrdBwmPolicy::Flow xF) : Next(0), refID(id),
             Way(xF == XrdBwmPolicy::Incoming ? In : Out) {}
      ~refReq() {}
      };

private:

class refSch
      {public:

       refReq  *First;
       refReq  *Last;
       int      Num;
       int      curSlots;
       int      maxSlots;

       void     Add(refReq *rP)
                       {if ((rP->Next = Last)) Last = rP;
                           else         First= Last = rP;
                        Num++;
                       }

       refReq  *Next() {refReq *rP;
                        if ((rP = First) && curSlots)
                           {if (!(First = First->Next)) Last = 0; 
                            Num--; curSlots--;
                           }
                        return rP;
                       }

       refReq  *Yank(int rID)
                       {refReq *pP = 0, *rP = First;
                        while(rP && rID != rP->refID) {pP = rP; rP = rP->Next;}
                        if (rP)
                           {if (pP) pP->Next = rP->Next;
                               else    First = rP->Next;
                            if (rP == Last) Last = pP;
                            Num--;
                           }
                         return rP;
                        }

                refSch() : First(0), Last(0), Num(0) {}
               ~refSch() {} // Never deleted!
      }         theQ[IOX];

XrdSysSemaphore pSem;
XrdSysMutex     pMutex;
int             refID;
};
#endif

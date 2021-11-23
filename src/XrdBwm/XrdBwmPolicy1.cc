/******************************************************************************/
/*                                                                            */
/*                      X r d B w m P o l i c y 1 . c c                       */
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

#include <cstring>

#include "XrdBwm/XrdBwmPolicy1.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdBwmPolicy1::XrdBwmPolicy1(int inslots, int outslots)
{
// Initialize values
//
   theQ[In ].maxSlots = theQ[In ].curSlots  =  inslots;
   theQ[Out].maxSlots = theQ[Out].curSlots  = outslots;
   theQ[Xeq].maxSlots = theQ[Xeq].curSlots  = 0;
   refID   = 1;
}

/******************************************************************************/
/*                              D i s p a t c h                               */
/******************************************************************************/
  
int  XrdBwmPolicy1::Dispatch(char *RespBuff, int RespSize)
{
   refReq *rP;
   int     rID;

// Obtain mutex and check if we have any queued requests
//
   do {pMutex.Lock();
       if ((rP = theQ[In].Next()) || (rP = theQ[Out].Next()))
          {theQ[Xeq].Add(rP);
           rID = rP->refID; *RespBuff = '\0';
           pMutex.UnLock();
           return rID;
          }
       pMutex.UnLock();
       pSem.Wait();
      } while(1);

// Should never get here
//
   strcpy(RespBuff, "Fatal logic error!");
   return 0;
}

/******************************************************************************/
/*                                  D o n e                                   */
/******************************************************************************/
  
int  XrdBwmPolicy1::Done(int rHandle)
{
   refReq *rP;
   int rc;

// Make sure we have a positive value here
//
   if (rHandle < 0) rHandle = -rHandle;

// Remove the element from whichever queue it is in
//
   pMutex.Lock();
   if ((rP = theQ[Xeq].Yank(rHandle)))
      {if (theQ[rP->Way].curSlots++ == 0) pSem.Post();
       rc = 1;
      } else {
       if ((rP=theQ[In].Yank(rHandle)) || (rP=theQ[Out].Yank(rHandle))) rc = -1;
          else rc = 0;
      }
    pMutex.UnLock();

// delete the element and return
//
   if (rP) delete rP;
   return rc;
}

/******************************************************************************/
/*                              S c h e d u l e                               */
/******************************************************************************/
  
int  XrdBwmPolicy1::Schedule(char *RespBuff, int RespSize, SchedParms &Parms)
{
   static const char *theWay[] = {"Incoming", "Outgoing"};
   refReq *rP;
   int myID;

// Get the global lock and generate a reference ID
//
   *RespBuff = '\0';
   pMutex.Lock();
   myID = ++refID;
   rP = new refReq(myID, Parms.Direction);

// Check if we can immediately schedule this requestor must defer it
//
        if (theQ[rP->Way].curSlots > 0)
           {theQ[rP->Way].curSlots--;
            theQ[Xeq].Add(rP);
           }
   else if (theQ[rP->Way].maxSlots) 
           {theQ[rP->Way].Add(rP); myID = -myID;}
   else {strcpy(RespBuff, theWay[rP->Way]);
         strcat(RespBuff, " requests are not allowed.");
         delete rP;
         myID = 0;
        }

// All done
//
   pMutex.UnLock();
   return myID;
}

/******************************************************************************/
/*                                S t a t u s                                 */
/******************************************************************************/
  
void XrdBwmPolicy1::Status(int &numqIn, int &numqOut, int &numXeq)
{

// Get the global lock and return the values
//
   pMutex.Lock();
   numqIn  = theQ[In ].Num;
   numqOut = theQ[Out].Num;
   numXeq  = theQ[Xeq].Num;
   pMutex.UnLock();
}

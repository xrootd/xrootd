/******************************************************************************/
/*                                                                            */
/*                       X r d X r o o t d P i o . c c                        */
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

#include "XrdXrootd/XrdXrootdPio.hh"

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdSysMutex        XrdXrootdPio::myMutex;
XrdXrootdPio      *XrdXrootdPio::Free = 0;
int                XrdXrootdPio::FreeNum = 0;

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdPio *XrdXrootdPio::Alloc(int Num)
{
   XrdXrootdPio *lqp, *qp=0;


// Allocate from the free stack
//
   myMutex.Lock();
   if ((qp = Free))
      {do {FreeNum--; Num--; lqp = Free;}
          while((Free = Free->Next) && Num);
       lqp->Next = 0;
      }
   myMutex.UnLock();

// Allocate additional if we have not allocated enough
//
   while(Num--) qp = new XrdXrootdPio(qp);

// All done
//
   return qp;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdXrootdPio::Recycle()
{

// Check if we can hold on to this or must delete it
//
   myMutex.Lock();
   if (FreeNum >= FreeMax) {myMutex.UnLock(); delete this; return;}

// Clean this up and push the element on the free stack
//
   Free = Clear(Free); FreeNum++;
   myMutex.UnLock();
}

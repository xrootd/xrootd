/******************************************************************************/
/*                                                                            */
/*                       X r d C m s R T a b l e . c c                        */
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

#include "XrdCms/XrdCmsRTable.hh"
#include "XrdCms/XrdCmsTrace.hh"

using namespace XrdCms;

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdCmsRTable XrdCms::RTable;

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
short XrdCmsRTable::Add(XrdCmsNode *nP)
{
   int i;

// Find a free slot for this node.
//
   myMutex.Lock();
   for (i = 1; i < maxRD; i++) if (!Rtable[i]) break;

// Insert the node if found
//
   if (i >= maxRD) i = 0;
      else {Rtable[i] = nP;
            if (i > Hwm) Hwm = i;
           }

// All done
//
   myMutex.UnLock();
   return static_cast<short>(i);
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdCmsRTable::Del(XrdCmsNode *nP)
{
   int i;

// Find the slot for this node.
//
   myMutex.Lock();
   for (i = 1; i <= Hwm; i++) if (Rtable[i] == nP) break;

// Remove the node if found
//
   if (i <= Hwm)
      {Rtable[i] = 0;
       if (i == Hwm) {while(--i) if (Rtable[i]) break; Hwm = i;}
      }

// All done
//
   myMutex.UnLock();
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/

// Note that the caller *must* call Lock() prior to calling find. We do this
// because this is the only way we can interlock the use of the node object
// with deletion of that object as it must be removed prior to deletion.

XrdCmsNode *XrdCmsRTable::Find(short Num, int Inst)
{

// Find the instance of the node in the indicated slot
//
   if (Num <= Hwm && Rtable[Num] && Rtable[Num]->Inst() == Inst)
      return Rtable[Num];
   return (XrdCmsNode *)0;
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
void XrdCmsRTable::Send(const char *What, const char *data, int dlen)
{
   EPNAME("Send");
   int i;

// Send the data to all nodes in this table
//
   myMutex.Lock();
   for (i = 1; i <= Hwm; i++) 
       if (Rtable[i])
          {DEBUG(What <<" to " <<Rtable[i]->Ident);
           Rtable[i]->Send(data, dlen);
          }
   myMutex.UnLock();
}

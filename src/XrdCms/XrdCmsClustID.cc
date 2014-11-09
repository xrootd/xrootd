/******************************************************************************/
/*                                                                            */
/*                      X r d C m s C l u s t I D . c c                       */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include <stdio.h>

#include "XrdCms/XrdCmsClustID.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"

using namespace XrdCms;

/******************************************************************************/
/*                  L o c a l   S t a t i c   O b j e c t s                   */
/******************************************************************************/
  
namespace
{
XrdSysMutex               cidMtx;

XrdOucHash<XrdCmsClustID> cidTab;

XrdCmsClustID            *cidFree = new XrdCmsClustID();
};

/******************************************************************************/
/* Static:                         A d d I D                                  */
/******************************************************************************/
  
XrdCmsClustID *XrdCmsClustID::AddID(const char *cID)
{
   XrdCmsClustID *cidObj;
   const char *cHN;
   char *clustID;

// Massage the clusterid (it's in bi-compatible format)
//
   if ((cHN = rindex(cID, ' ')) && *(cHN+1)) cID = cHN+1;
   clustID = strdup(cID);

// Lock ourselves
//
   cidMtx.Lock();

// Allocate a new cluster ID object if we don't have one ready
//
   if (!cidFree) cidFree = new XrdCmsClustID();

// Attempt to add this object to our cid table
//
   if (!(cidObj = cidTab.Add(clustID, cidFree, 0, Hash_keep)))
      {cidObj = cidFree;
       cidObj->cidName = clustID;
       cidFree = new XrdCmsClustID();
      } else free(clustID);

// We can unlock now
//
   cidMtx.UnLock();

// Return the entry
//
   return cidObj;
}
  
/******************************************************************************/
/*                               A d d N o d e                                */
/******************************************************************************/
  
bool XrdCmsClustID::AddNode(XrdCmsNode *nP, bool isMan)
{
   EPNAME("AddNode");
   XrdSysMutexHelper cidHelper(cidMtx);
   int iNum, sNum;

// For servers we only add the identification mask
//
   if (!isMan)
      {cidMask |= nP->Mask();
       DEBUG("srv " <<nP->Ident <<" cluster " <<cidName
             <<" mask=" <<hex <<cidMask <<dec <<" anum=" <<npNum);
       return true;
      }

// Make sure we have enough space in the table
//
   if (npNum >= altMax)
      {Say.Emsg("ClustID",cidName,"alternate table full; rejecting",nP->Name());
       return false;
      }

// Make sure the slot numbers match for this node
//
   sNum = nP->ID(iNum);
   if (npNum > 0 && ntSlot != sNum)
      {char buff[256];
       sprintf(buff,"cluster slot mismatch: %d != %d; rejecting",sNum,ntSlot);
       Say.Emsg("ClustID", cidName, buff, nP->Name());
       return false;
      }

// Add the entry to the table
//
   ntSlot = sNum;
   cidMask |= nP->Mask();
   nodeP[npNum++] = nP;
   DEBUG("man " <<nP->Ident <<" cluster " <<cidName
         <<" mask=" <<hex <<cidMask <<dec <<" anum=" <<npNum);
   return true;
}

/******************************************************************************/
/*                                E x i s t s                                 */
/******************************************************************************/
  
bool XrdCmsClustID::Exists(XrdLink *lp, const char *nid, int port)
{
   XrdSysMutexHelper cidHelper(cidMtx);

// Simply scan the table to see if this node is present
//
   for (int i = 0; i <npNum; i++)
       {if (nodeP[i]->isNode(lp, nid, port)) return true;}
   return false;
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
XrdCmsClustID *XrdCmsClustID::Find(const char *cID)
{
   XrdCmsClustID *cidObj;
   const char *cHN;

// Massage the clusterid (it's in bi-compatible format)
//
   if ((cHN = rindex(cID, ' ')) && *(cHN+1)) cID = cHN+1;

// Lock ourselves
//
   cidMtx.Lock();

// Attempt to find the cluster object
//
   cidObj = cidTab.Find(cID);

// We can unlock now
//
   cidMtx.UnLock();

// Return the entry
//
   return cidObj;
}

/******************************************************************************/
/*                                  M a s k                                   */
/******************************************************************************/
  
SMask_t XrdCmsClustID::Mask(const char *cID)
{
   XrdCmsClustID *cidObj;
   SMask_t        theMask;
   const char *cHN;

// Massage the clusterid (it's in bi-compatible format)
//
   if ((cHN = rindex(cID, ' ')) && *(cHN+1)) cID = cHN+1;

// Lock ourselves
//
   cidMtx.Lock();

// Attempt to find the cluster object
//
   if ((cidObj = cidTab.Find(cID))) theMask = cidObj->cidMask;
      else theMask = 0;

// We can unlock now
//
   cidMtx.UnLock();

// Return the mask
//
   return theMask;
}

/******************************************************************************/
/*                               R e m N o d e                                */
/******************************************************************************/
  
XrdCmsNode *XrdCmsClustID::RemNode(XrdCmsNode *nP)
{
   EPNAME("RemNode");
   bool didRM = false;

// For servers we only need to remove the mask
//
   if (!(nP->isMan | nP->isPeer))
      {cidMask &= ~(nP->Mask());
       DEBUG("srv " <<nP->Ident <<" cluster " <<cidName
             <<" mask=" <<hex <<cidMask <<dec <<" anum=" <<npNum);
       return 0;
      }

// Find the node to remove. This may require a fill in.
//
   for (int i = 0; i < npNum; i++)
       if (nP == nodeP[i])
          {npNum--;
           if (i < npNum && npNum) nodeP[i] = nodeP[npNum];
              else nodeP[i] = 0;
           didRM = true;
           break;
          }

// If there are no more nodes in this table, then remove the id mask
//
   if (!npNum) cidMask &= ~(nP->Mask());

// Do some debugging and return what we have in the table
//
   DEBUG("man " <<nP->Ident <<" cluster " <<cidName
         <<" mask=" <<hex <<cidMask <<dec <<" anum=" <<npNum
         <<(didRM ? "" : " n/p"));
   return (npNum ? nodeP[0] : 0);
}

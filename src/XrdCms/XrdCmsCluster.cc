/******************************************************************************/
/*                                                                            */
/*                      X r d C m s C l u s t e r . c c                       */
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

#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "XProtocol/YProtocol.hh"
  
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdCms/XrdCmsBaseFS.hh"
#include "XrdCms/XrdCmsBlackList.hh"
#include "XrdCms/XrdCmsCache.hh"
#include "XrdCms/XrdCmsConfig.hh"
#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsClustID.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsRole.hh"
#include "XrdCms/XrdCmsRRQ.hh"
#include "XrdCms/XrdCmsState.hh"
#include "XrdCms/XrdCmsSelect.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdCms/XrdCmsTypes.hh"

#include "XrdOuc/XrdOucPup.hh"

#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdCms;

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

       XrdCmsCluster   XrdCms::Cluster;

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
class XrdCmsDrop : XrdJob
{
public:

     void DoIt() {if (nodeP)
                     {nodeP->Delete(Cluster.STMutex);
                      delete this;
                     } else {
                      if (!Cluster.Drop(nodeEnt, nodeInst, this)) delete this;
                     }
                 }

          XrdCmsDrop(XrdCmsNode *nP) : XrdJob("delete node"), nodeP(nP),
                                       nodeEnt(0), nodeInst(0)
                    {Sched->Schedule((XrdJob *)this);}

          XrdCmsDrop(int nid, int inst) : XrdJob("drop node"), nodeP(0),
                                          nodeEnt(nid), nodeInst(inst)
                    {Sched->Schedule((XrdJob *)this, time(0)+Config.DRPDelay);}

         ~XrdCmsDrop() {}

XrdCmsNode *nodeP;
int         nodeEnt;
int         nodeInst;
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdCmsCluster::XrdCmsCluster()
{
     memset((void *)NodeTab, 0, sizeof(NodeTab));
     memset((void *)AltMans, (int)' ', sizeof(AltMans));
     AltMend = AltMans;
     AltMent = -1;
     NodeCnt =  0;
     STHi    = -1;
     SelWtot = 0;
     SelRtot = 0;
     SelTcnt = 0;
     peerHost  = 0;
     peerMask  = ~peerHost;
}
  
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
XrdCmsNode *XrdCmsCluster::Add(XrdLink *lp, int port, int Status, int sport,
                               const char *theNID, const char *theIF)

{
   EPNAME("Add")
   const char *act = "";
   XrdCmsNode *nP = 0;
   XrdCmsClustID *cidP = 0;
   XrdSysRWLockHelper STMHelper(STMutex, false); // Need write lock!
   int tmp, Slot, Free = -1, Bump1 = -1, Bump2 = -1, Bump3 = -1, aSet = 0;
   bool Special = (Status & (CMS_isMan|CMS_isPeer));
   bool SpecAlt = (Special && !(Status & CMS_isSuper));
   bool Hidden  = false;

// Find available slot for this node. Here are the priorities:
// Slot  = Reconnecting node
// Free  = Available slot           ( 1st in table)
// Bump1 = Disconnected server      (last in table)
// Bump2 = Connected    server      (last in table) if new one is managr/peer
// Bump3 = Disconnected managr/peer ( 1st in table) if new one is managr/peer
//
   for (Slot = 0; Slot < STMax; Slot++)
       if (NodeTab[Slot])
          {if (NodeTab[Slot]->isNode(lp, theNID, port)) break;
/*Conn*/   if (NodeTab[Slot]->isConn)
              {if (!NodeTab[Slot]->isPerm && Special)
                                             Bump2 = Slot; // Last conn Server
/*Disc*/      } else {
               if ( NodeTab[Slot]->isPerm)
                  {if (Bump3 < 0 && Special) Bump3 = Slot;}//  1st disc Man/Pr
                  else                       Bump1 = Slot; // Last disc Server
              }
          } else if (Free < 0)               Free  = Slot; //  1st free slot

// Check if node is already logged in or is a relogin
//
   if (Slot < STMax)
      {if (NodeTab[Slot] && NodeTab[Slot]->isBound)
          {Say.Emsg("Cluster", lp->ID, "already logged in.");
           return 0;
          } else { // Rehook node to previous unconnected entry
           nP = NodeTab[Slot];
           nP->Link      = lp;
           nP->isOffline = 0;
           nP->isBad    &= ~XrdCmsNode::isSuspend;
           nP->isConn    = 1;
           nP->Instance++;
           nP->setName(lp, theIF, port);  // Just in case it changed
           act = "Reconnect ";
          }
      }

// First see if this node may be an alternate
//
   if (!nP && SpecAlt)
      {if ((cidP = XrdCmsClustID::Find(theNID)) && !(cidP->IsEmpty()))
          {if (!(nP = AddAlt(cidP, lp, port, Status, sport, theNID, theIF)))
              return 0;
           aSet = 1; Slot = nP->NodeID;
           if (nP != NodeTab[Slot]) {Hidden = true; act = "Alternate ";}
          }
      }

// Reuse an old ID if we must or redirect the incoming node
//
   if (!nP) 
      {if (Free >= 0) Slot = Free;
          else {if (Bump1 >= 0) Slot = Bump1;
                   else Slot = (Bump2 >= 0 ? Bump2 : Bump3);
                if (Slot < 0)
                   {if (Status & CMS_isPeer) Say.Emsg("Cluster", "Add peer", lp->ID,
                                                "failed; too many subscribers.");
                       else {sendAList(lp);
                             DEBUG(lp->ID <<" redirected; too many subscribers.");
                            }
                    return 0;
                   }

                if (Status & CMS_isMan) {setAltMan(Slot, lp, sport); aSet=1;}
                if (NodeTab[Slot] && !(Status & CMS_isPeer))
                   sendAList(NodeTab[Slot]->Link);

                DEBUG(lp->ID << " bumps " << NodeTab[Slot]->Ident <<" #" <<Slot);
                NodeTab[Slot]->Lock();
                Remove("redirected", NodeTab[Slot], -1);
                act = "Shoved ";
               }
       NodeTab[Slot] = nP = new XrdCmsNode(lp, theIF, theNID, port, 0, Slot);
       if (!cidP) cidP = XrdCmsClustID::AddID(theNID);
       if ((cidP->AddNode(nP, SpecAlt))) nP->cidP = cidP;
          else {delete nP; NodeTab[Slot] = 0; return 0;} // OK to do delete!
      }

// Indicate whether this snode can be redirected
//
   nP->isPerm = (Status & (CMS_isMan | CMS_isPeer)) ? 1 : 0;

// Assign new server
//
   if (!aSet && (Status & CMS_isSuper)) setAltMan(Slot, lp, sport);
   if (Slot > STHi) STHi = Slot;
   nP->isBound   = 1;
   nP->isConn    = 1;
   nP->isNoStage = 0 != (Status & CMS_noStage);
   nP->isBad    |=      (Status & CMS_Suspend ? XrdCmsNode::isSuspend : 0);
   nP->isMan     = 0 != (Status & CMS_isMan);
   nP->isPeer    = 0 != (Status & CMS_isPeer);
   nP->isBad    |= XrdCmsNode::isDisabled;
   nP->subsPort  = sport;

// If this is an actual non-hidden node, count it
//
   if (!Hidden)
      {NodeCnt++;
       if (Config.SUPLevel
       && (tmp = NodeCnt*Config.SUPLevel/100) > Config.SUPCount)
          {Config.SUPCount=tmp; CmsState.Set(tmp);}
      } else nP->isMan |= 0x02;

// Compute new peer mask, as needed
//
   if (nP->isPeer) peerHost |=  nP->NodeMask;
      else         peerHost &= ~nP->NodeMask;
   peerMask = ~peerHost;

// Document login
//
   if (QTRACE(Debug))
      {DEBUG(act <<nP->Ident <<" to cluster " <<nP->myNID <<" slot "
             <<Slot <<'.' <<nP->Instance <<" (nodecnt=" <<NodeCnt
             <<" supn=" <<Config.SUPCount <<")");
      }

// Compute new state of all nodes if we are a reporting manager.
//
   if (Config.asManager() && !Hidden)
      CmsState.Update(XrdCmsState::Counts,
                      nP->isBad & XrdCmsNode::isSuspend ? 0 : 1,
                      nP->isNoStage ? 0 : 1);

// All done. Return the node locked.
//
   nP->Lock();
   return nP;
}
  
/******************************************************************************/
/* Private:                       A d d A l t                                 */
/******************************************************************************/
  
// Warning STMutex must be held in write mode by the caller!

XrdCmsNode *XrdCmsCluster::AddAlt(XrdCmsClustID *cidP, XrdLink *lp,
                                  int port, int Status, int sport,
                                  const char *theNID, const char *theIF)

{
   EPNAME("AddAlt")
   XrdCmsNode *pP, *nP = 0;
   int slot = cidP->Slot();

// Check if this node is already in the alternate table
//
   if (cidP->Exists(lp, theNID, port))
      {Say.Emsg(epname, lp->ID, "already logged in.");
       return 0;
      }

// Add this node if there is room
//
   if (cidP->Avail())
      {nP = new XrdCmsNode(lp, theIF, theNID, port, 0, slot);
       if (!(cidP->AddNode(nP, true))) {delete nP; nP = 0;} // OK to do delete!
      }

// Check if we were actually able to add this node
//
   if (!nP)
      {Say.Emsg(epname, "Add alternate manager", lp->ID,
                        "failed; too many subscribers.");
       return 0;
      }

// Check if the existing lead dead and we can substiture this one
//
   if ((pP = NodeTab[slot]) && !(pP->isBound))
      {setAltMan(nP->NodeID, nP->Link, sport);
       Say.Emsg("AddAlt", nP->Ident, "replacing dropped", pP->Ident);
       NodeTab[slot] = nP;
       pP->DropJob = new XrdCmsDrop(pP); // Schedule deletion
      }

// Hook the node to the cluster table and return
//
   nP->cidP = cidP;
   return nP;
}

/******************************************************************************/
/*                             B l a c k L i s t                              */
/******************************************************************************/

void XrdCmsCluster::BlackList(XrdOucTList *blP)
{
   static CmsDiscRequest discRequest = {{0, kYR_disc, 0, 0}};
   XrdCmsNode *nP;
   const char *etxt = "blacklisted.";
   int i, blRD = 0;
   bool inBL;

// Obtain a lock on the table. We need this in write mode!
//
   STMutex.WriteLock();

// Run through the table looking to put or out of the blacklist
//
   for (i = 0; i <= STHi; i++)
       {if ((nP = NodeTab[i]))
           {inBL = (blP && (blRD = XrdCmsBlackList::Present(nP->Name(), blP)));
            if ((!inBL && !(nP->isBad & XrdCmsNode::isBlisted))
            ||  ( inBL &&  (nP->isBad & XrdCmsNode::isBlisted))) continue;
            nP->g2nLock(STMutex); // Downgrade to only node lock
            if (inBL)
               {nP->isBad |= XrdCmsNode::isBlisted | XrdCmsNode::isDoomed;
                if (blRD < -1)
                  {if (kYR_Version > nP->myVersion)
                            etxt = "blacklisted; redirect unsupported.";
                      else  etxt = "blacklisted with redirect.";
                   nP->isBad |= XrdCmsNode::isDoomed;
                   nP->Send((char *)&discRequest, sizeof(discRequest));
                  }
                Say.Emsg("Manager", nP->Name(), etxt);
               } else {
                nP->isBad &= ~(XrdCmsNode::isBlisted | XrdCmsNode::isDoomed);
                Say.Emsg("Manager", nP->Name(), "removed from blacklist.");
               }
            nP->n2gLock(STMutex);
           }
       }
   STMutex.UnLock();
}
  
/******************************************************************************/
/*                             B r o a d c a s t                              */
/******************************************************************************/

SMask_t XrdCmsCluster::Broadcast(SMask_t smask, const struct iovec *iod,
                                 int iovcnt, int iotot)
{
   EPNAME("Broadcast")
   int i;
   XrdCmsNode *nP;
   SMask_t bmask, unQueried(0);

// Obtain a lock on the table and screen out peer nodes
//
   STMutex.ReadLock(); // Sufficient to prevent modifications
   bmask = smask & peerMask;

// Run through the table looking for nodes to send messages to. We don't need
// the node lock for this but we do need to up the reference count to keep the
// node pointer valid for the duration of the send() (may or may not block).
//
   for (i = 0; i <= STHi; i++)
       {if ((nP = NodeTab[i]) && nP->isNode(bmask))
           {if (nP->isOffline) unQueried |= nP->Mask();
               else {nP->Ref();
                     STMutex.UnLock();
                     if (nP->Send(iod, iovcnt, iotot) < 0)
                        {unQueried |= nP->Mask();
                         DEBUG(nP->Ident <<" is unreachable");
                        }
                     nP->unRef();
                     STMutex.ReadLock();
                    }
           }
       }
   STMutex.UnLock();
   return unQueried;
}

/******************************************************************************/

SMask_t XrdCmsCluster::Broadcast(SMask_t smask, XrdCms::CmsRRHdr &Hdr,
                                 char *Data,    int Dlen)
{
   struct iovec ioV[3], *iovP = &ioV[1];
   unsigned short Temp;
   int Blen;

// Construct packed data for the character argument. If data is a string then
// Dlen must include the null byte if it is specified at all.
//
   Blen  = XrdOucPup::Pack(&iovP, Data, Temp, (Dlen ? strlen(Data)+1 : Dlen));
   Hdr.datalen = htons(static_cast<unsigned short>(Blen));

// Complete the iovec and send off the data
//
   ioV[0].iov_base = (char *)&Hdr; ioV[0].iov_len = sizeof(Hdr);
   return Broadcast(smask, ioV, 3, Blen+sizeof(Hdr));
}

/******************************************************************************/

SMask_t XrdCmsCluster::Broadcast(SMask_t smask, XrdCms::CmsRRHdr &Hdr,
                                 void *Data,    int Dlen)
{
   struct iovec ioV[2] = {{(char *)&Hdr, sizeof(Hdr)},
                          {(char *)Data, (size_t)Dlen}};

// Send of the data as eveything was constructed properly
//
   Hdr.datalen = htons(static_cast<unsigned short>(Dlen));
   return Broadcast(smask, ioV, 2, Dlen+sizeof(Hdr));
}

/******************************************************************************/
/*                             B r o a d s e n d                              */
/******************************************************************************/

// Send message to first eligible node!

int XrdCmsCluster::Broadsend(SMask_t Who, XrdCms::CmsRRHdr &Hdr, 
                             void *Data, int Dlen)
{
   EPNAME("Broadsend");
   static int Start = 0;
   XrdCmsNode *nP;
   struct iovec ioV[2] = {{(char *)&Hdr, sizeof(Hdr)},
                          {(char *)Data, (size_t)Dlen}};
   int i, Beg, Fin, ioTot = Dlen+sizeof(Hdr);

// Send of the data as eveything was constructed properly
//
   Hdr.datalen = htons(static_cast<unsigned short>(Dlen));

// Obtain a lock on the table and get the starting and ending position. Note
// that the mechnism we use will necessarily skip newly added nodes.
//
   STMutex.ReadLock(); // Sufficient to prevent modifications
   Beg = Start = (Start <= STHi ? Start+1 : 0);
   Fin = STHi;

// Run through the table looking for a node to send a message to. We don't need
// the node lock for this but we do need to up the reference count to keep the
// node pointer valid for the duration of the send() (may or may not block).
//
do{for (i = Beg; i <= Fin; i++)
       {if ((nP = NodeTab[i]) && nP->isNode(Who))
           {if (nP->isOffline) continue;
            nP->Ref();
            STMutex.UnLock();
            if (nP->Send(ioV, 2, ioTot) >= 0) {nP->unRef(); return 1;}
            DEBUG(nP->Ident <<" is unreachable");
            nP->unRef();
            STMutex.ReadLock();
           }
       }
    if (!Beg) break;
    Fin = Beg-1; Beg = 0;
   } while(1);

// Did not send to anyone
//
   STMutex.UnLock();
   return 0;
}
  
/******************************************************************************/
/*                               g e t M a s k                                */
/******************************************************************************/

SMask_t XrdCmsCluster::getMask(const XrdNetAddr *addr)
{
   int i;
   XrdCmsNode *nP;
   SMask_t smask(0);

// Obtain a lock on the table
//
   STMutex.ReadLock();

// Run through the table looking for a node with matching IP address
//
   for (i = 0; i <= STHi; i++)
       if ((nP = NodeTab[i]) && nP->isNode(addr))
          {smask = nP->NodeMask; break;}

// All done
//
   STMutex.UnLock();
   return smask;
}

/******************************************************************************/

SMask_t XrdCmsCluster::getMask(const char *Cid)
{
   return XrdCmsClustID::Mask(Cid);
}

/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/

XrdCmsSelected *XrdCmsCluster::List(SMask_t mask, CmsLSOpts opts, bool &oksel)
{
    static const int iSize = XrdCmsSelected::IdentSize;
    XrdCmsNode      *nP;
    XrdCmsSelected  *sipp = 0, *sip;
    XrdNetIF::ifType ifType = (XrdNetIF::ifType)(opts & LS_IFMASK);
    XrdNetIF::ifType ifGet  = ifType;
    int i, destLen;
    bool retName = (opts & LS_IDNT) != 0;
    bool retAny  = (opts & LS_ANY ) != 0;
    bool retDest = retName || (opts & LS_IPO);

// If only one wanted, the select appropriately
//
   oksel = false;
   STMutex.ReadLock();
   for (i = 0; i <= STHi; i++)
        if ((nP=NodeTab[i]) && (nP->NodeMask & mask))
           {oksel = true;
            if (retDest)
               {     if (nP->netIF.HasDest(ifType)) ifGet = ifType;
                else if (!retAny) continue;
                else {ifGet = (XrdNetIF::ifType)(ifType ^ XrdNetIF::PrivateIF);
                      if (!nP->netIF.HasDest(ifGet)) continue;
                     }
               }
            sip = new XrdCmsSelected(sipp);
                 if (retDest) destLen = nP->netIF.GetDest(sip->Ident, iSize,
                                                          ifGet, retName);
            else if (nP->myNlen >= XrdCmsSelected::IdentSize) destLen = 0;
            else {strcpy(sip->Ident, nP->myName); destLen = nP->myNlen;}
            if (!destLen) {delete sip; continue;}

            sip->IdentLen = destLen;
            sip->Mask    = nP->NodeMask;
            sip->Id      = nP->NodeID;
            sip->Port    = nP->netIF.Port();
            sip->RefTotW = nP->RefTotW;
            sip->RefTotR = nP->RefTotR;
            sip->Shrin   = nP->Shrin;
            sip->Share   = nP->Share;
            sip->RoleID  = nP->RoleID;
            sip->Status  = (nP->isOffline ? XrdCmsSelected::Offline : 0);
            if (nP->isBad & (XrdCmsNode::isDisabled | XrdCmsNode::isBlisted))
               sip->Status |= XrdCmsSelected::Disable;
            if (nP->isNoStage) sip->Status |= XrdCmsSelected::NoStage;
            if (nP->isBad & XrdCmsNode::isSuspend)
               sip->Status |= XrdCmsSelected::Suspend;
            if (nP->isRW     ) sip->Status |= XrdCmsSelected::isRW;
            if (nP->isMan    ) sip->Status |= XrdCmsSelected::isMangr;
            sipp = sip;
           }
   STMutex.UnLock();

// Return result
//
   return sipp;
}
  
/******************************************************************************/
/*                                L o c a t e                                 */
/******************************************************************************/

int XrdCmsCluster::Locate(XrdCmsSelect &Sel)
{
   EPNAME("Locate");
   XrdCmsPInfo   pinfo;
   SMask_t       qfVec(0);
   char         *Path;
   int           retc = 0;

// Check if this is a locate for all current servers
//
   if (*Sel.Path.Val != '*') Path = Sel.Path.Val;
      else {if (*(Sel.Path.Val+1) == '\0')
               {Sel.Vec.hf = ~0LL; Sel.Vec.pf = Sel.Vec.wf = 0;
                return 0;
               }
            Path = Sel.Path.Val+1;
           }

// Find out who serves this path
//
   if (!Cache.Paths.Find(Path, pinfo) || !pinfo.rovec)
      {Sel.Vec.hf = Sel.Vec.pf = Sel.Vec.wf = 0;
       return NotFound;
      } else Sel.Vec.wf = pinfo.rwvec;

// Check if this was a non-lookup request
//
   if (*Sel.Path.Val == '*')
      {Sel.Vec.hf = pinfo.rovec; Sel.Vec.pf = 0;
       Sel.Vec.wf = pinfo.rwvec;
       return 0;
      }

// Complete the request info object if we have one
//
   if (Sel.InfoP)
      {Sel.InfoP->rwVec = pinfo.rwvec;
       Sel.InfoP->isLU  = 1;
      }

// If we are running a shared file system preform an optional restricted
// pre-selection and then do a standard selection.
//
   if (baseFS.isDFS())
      {SMask_t amask, smask, pmask;
       amask = pmask = pinfo.rovec;
       smask = (Sel.Opts & XrdCmsSelect::Online ? 0 : pinfo.ssvec & amask);
       Sel.Resp.DLen = 0;
       if (!(retc = SelDFS(Sel, amask, pmask, smask, 1)))
          return (Sel.Opts & XrdCmsSelect::Asap && Sel.InfoP
                ? Cache.WT4File(Sel,Sel.Vec.hf) : Config.LUPDelay);
       if (retc < 0) return NotFound;
       return 0;
      }

// First check if we have seen this file before. If so, get nodes that have it.
// A Refresh request kills this because it's as if we hadn't seen it before.
// If the file was found but either a query is in progress or we have a server
// bounce; the client must wait.
//
   if (Sel.Opts & XrdCmsSelect::Refresh 
   || !(retc = Cache.GetFile(Sel, pinfo.rovec)))
      {Cache.AddFile(Sel, 0);
       qfVec = pinfo.rovec; Sel.Vec.hf = 0;
      } else qfVec = Sel.Vec.bf;

// Compute the delay, if any
//
   if ((!qfVec && retc >= 0) || (Sel.Vec.hf && Sel.InfoP)) retc =  0;
      else if (!(retc = Cache.WT4File(Sel, Sel.Vec.hf)))   retc = Wait4CBk;

// Check if we have to ask any nodes if they have the file
//
   if (qfVec)
      {CmsStateRequest QReq = {{Sel.Path.Hash, kYR_state, kYR_raw, 0}};
       if (Sel.Opts & XrdCmsSelect::Refresh)
          QReq.Hdr.modifier |= CmsStateRequest::kYR_refresh;
       TRACE(Files, "seeking " <<Sel.Path.Val);
       qfVec = Cluster.Broadcast(qfVec, QReq.Hdr, 
                                 (void *)Sel.Path.Val, Sel.Path.Len+1);
       if (qfVec) Cache.UnkFile(Sel, qfVec);
      }
   return retc;
}
  
/******************************************************************************/
/*                               M o n P e r f                                */
/******************************************************************************/
  
void *XrdCmsCluster::MonPerf()
{
   CmsUsageRequest Usage = {{0, kYR_usage, 0, 0}};
   struct iovec ioV[] = {{(char *)&Usage, sizeof(Usage)}};
   int ioVnum = sizeof(ioV)/sizeof(struct iovec);
   int ioVtot = sizeof(Usage);
   SMask_t allNodes(~0);
   int uInterval = Config.AskPing*Config.AskPerf;

// Sleep for the indicated amount of time, then ask for load on each server
//
   while(uInterval)
        {XrdSysTimer::Snooze(uInterval);
         Broadcast(allNodes, ioV, ioVnum, ioVtot);
        }
   return (void *)0;
}
  
/******************************************************************************/
/*                               M o n R e f s                                */
/******************************************************************************/
  
void *XrdCmsCluster::MonRefs()
{
   XrdCmsNode *nP;
   int  snooze_interval = 60, snooze_total = 0;
   int  rCnt = 0, wCnt = 0;
   bool resetW, resetR, resetRW;

// Sleep for the snooze interval. If a reset was requested then do a selective
// reset unless we reached our snooze maximum and enough selections have gone
// by; in which case, do a global reset.
//
   do {XrdSysTimer::Snooze(snooze_interval);
       int totR = 0, totW = 0;

       STMutex.ReadLock();
       for (int i = 0; i <= STHi; i++)
           {if ((nP = NodeTab[i]))
               {totR += nP->RefTotR;
                totW += nP->RefTotW;
               }
           }
       STMutex.UnLock();

       rCnt += (totR - SelRtot); SelRtot = totR;
       wCnt += (totW - SelWtot); SelWtot = totW;
       snooze_total += snooze_interval;

       resetR  = (rCnt >= Config.RefTurn);
       resetW  = (wCnt >= Config.RefTurn);
       resetRW = (snooze_total >= Config.RefReset && (resetW || resetR));
       if (resetRW)
          {ResetRef((SMask_t)0);
           if (resetR) rCnt = 0;
           if (resetW) wCnt = 0;
           snooze_total = 0;
          }
      } while(1);

   return (void *)0;
}

/******************************************************************************/
/*                                R e m o v e                                 */
/******************************************************************************/

// Warning! The node object must be locked upon entry. The lock is released
//          upon deletion of the object.

void XrdCmsCluster::Remove(XrdCmsNode *theNode)
{
     theNode->DropJob  = new XrdCmsDrop(theNode);
}

// Warning! The node object must be locked upon entry. The lock is released
//          prior to returning to the caller. This entry obtains the node
//          table lock. When immed != 0 then the node is immediately dropped.
//          When immed if < 0 then the caller already holds the STMutex in
//          write mode and it is not released upon exit.

void XrdCmsCluster::Remove(const char *reason, XrdCmsNode *theNode, int immed)
{
   EPNAME("Remove_Node")
   struct theLocks
          {XrdSysRWLock *myMutex;
           XrdCmsNode   *myNode;
           int           myNID;
           int           myInst;
           bool          hasLK;
           bool          doDrop;
           char          myIdent[510];

                       theLocks(XrdSysRWLock *mtx, XrdCmsNode *node, int immed)
                               : myMutex(mtx), myNode(node), hasLK(immed < 0),
                                 doDrop(false)
                               {strlcpy(myIdent, node->Ident, sizeof(myIdent));
                                myNID = node->ID(myInst);
                                if (!hasLK)
                                   {myNode->Ref();        // Keep alive
                                    myNode->UnLock();
                                    myMutex->WriteLock(); // Get global lock
                                    myNode->Lock();
                                    myNode->unRef();      // Can't escape now
                                   }
                               }
                      ~theLocks()
                               {if (myNode)
                                   {if (doDrop)
                                       {myNode->isBound  = 0;
                                        myNode->DropTime = 0;
                                        myNode->DropJob  = new XrdCmsDrop(myNode);
                                        myNode->UnLock();
                                       } else myNode->UnLock();
                                   }
                                if (!hasLK) myMutex->UnLock();
                               }
          } LockHandler(&STMutex, theNode, immed);

   XrdCmsNode *altNode = 0;
   int Inst, NodeID = theNode->ID(Inst);

// The LockHandler makes sure that the proper locks are obtained in a deadlock
// free order. However, this may require that the node lock be released and
// then re-aquired. We check if we are still dealing with same node at entry.
// If not, issue message and high-tail it out.
//
   if (LockHandler.myNID != NodeID || LockHandler.myInst != Inst)
      {Say.Emsg("Manager", LockHandler.myIdent, "removal aborted.");
       DEBUG(LockHandler.myIdent <<" node " <<NodeID <<'.' <<Inst <<" != "
             << LockHandler.myNID <<'.' <<LockHandler.myInst <<" at entry.");
      }

// Mark node as being offline and remove any drop job from it
//
   theNode->isOffline = 1; // Global lock is held here

// If the node is connected we simply close the connection. This will cause
// the connection handler to re-initiate the node removal. This condition
// exists only if one node is being displaced by another node. The Disc()
// may take a long time, but it's done async by default on the WAN and sync
// on the LAN (local connections are fast enough and error-free for this).
//
   if (theNode->isConn)
      {theNode->Disc(reason, 0);
       theNode->isGone = 1; // Disc() sets the isOffline flag
       return;
      }

// If we are not the primary node, then get rid of this node post-haste
//
   if (!(NodeTab[NodeID] == theNode))
      {const char *why = (theNode->isMan ? "dropped as alternate."
                                         : "dropped and redirected.");
       Say.Emsg("Remove_Node", theNode->Ident, why);
       LockHandler.doDrop = true;
       return;
      }


// If the node is part of the cluster, do not count it anymore and
// indicate new state of this nodes if we are a reporting manager
//
   if (theNode->isBound)
      {theNode->isBound = 0;
       NodeCnt--;
       if (Config.asManager())
          CmsState.Update(XrdCmsState::Counts,
                          theNode->isBad & XrdCmsNode::isSuspend ? 0 : -1,
                          theNode->isNoStage ? 0 : -1);
      }

// If we have a working alternate, substitute it here and immediately drop
// the former primary. This allows the cache to remain warm.
//
   if (theNode->isMan && theNode->cidP && !(theNode->cidP->IsSingle())
   && (altNode = theNode->cidP->RemNode(theNode)))
      {if (altNode->isBound) NodeCnt++;
       NodeTab[NodeID] = altNode;
       if (Config.asManager())
          CmsState.Update(XrdCmsState::Counts,
                          altNode->isBad & XrdCmsNode::isSuspend ? 0 :  1,
                          altNode->isNoStage ? 0 :  1);
       setAltMan(altNode->NodeID, altNode->Link, altNode->subsPort);
       Say.Emsg("Manager",altNode->Ident,"replacing dropped",theNode->Ident);
       LockHandler.doDrop = true;
       return;
      }

// If this is an immediate drop request, do so now. Drop() will delete
// the node object, so remove the node lock and tell LockHandler that.
//
   if (immed || !Config.DRPDelay || theNode->isBad & XrdCmsNode::isDoomed)
      {theNode->UnLock();
       LockHandler.myNode = 0;
       Drop(NodeID, Inst);
       return;
      }

// If a drop job is already scheduled, update the instance field. Otherwise,
// Schedule a node drop at a future time.
//
   theNode->DropTime = time(0)+Config.DRPDelay;
   if (theNode->DropJob) theNode->DropJob->nodeInst = Inst;
      else theNode->DropJob = new XrdCmsDrop(NodeID, Inst);

// Document removal
//
   if (reason) 
      Say.Emsg("Manager", theNode->Ident, "scheduled for removal;", reason);
      else DEBUG(theNode->Ident <<" node " <<NodeID <<'.' <<Inst);
}

/******************************************************************************/
/*                              R e s e t R e f                               */
/******************************************************************************/
  
void XrdCmsCluster::ResetRef(SMask_t nMask, bool isLocked)
{
   XrdCmsNode *nP;
   bool doAll (nMask == 0);

// Obtain a lock on the table if not already locked
//
   if (!isLocked) STMutex.ReadLock();

// Reset reference counts as needed. We can do this with a read lock as the
// reference counters are atomic.
//
   for (int i = 0; i <= STHi; i++)
       {if ((nP = NodeTab[i]) && (doAll || nP->isNode(nMask)))
           {nP->RefW  = 0;
            nP->RefR  = 0;
            nP->Shrem = nP->Share;
           }
       }

// Unlock table and exit
//
   if (!isLocked) STMutex.UnLock();
}

/******************************************************************************/
/*                                S e l e c t                                 */
/******************************************************************************/
  
int XrdCmsCluster::Select(XrdCmsSelect &Sel)
{
   EPNAME("Select");
   XrdCmsPInfo  pinfo;
   const char  *Amode;
   int dowt = 0, retc = 0, isRW, fRD, noSel = (Sel.Opts & XrdCmsSelect::Defer);
   SMask_t amask, smask, pmask;

// Establish some local options
//
   if (Sel.Opts & XrdCmsSelect::Write) 
      {isRW = 1; Amode = "write";
       if (Config.RWDelay)
          if (Sel.Opts & XrdCmsSelect::Create && Config.RWDelay < 2) fRD = 1;
             else fRD = 0;
          else fRD = 1;
      }
      else {isRW = 0; Amode = "read"; fRD = 1;}

// Find out who serves this path
//
   if (!Cache.Paths.Find(Sel.Path.Val, pinfo)
   || (amask = ((isRW ? pinfo.rwvec : pinfo.rovec) & ~Sel.nmask)) == 0)
      {Sel.Resp.DLen = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data)-1,
                       "No servers have %s access to the file", Amode)+1;
       Sel.Resp.Port = kYR_ENOENT;
       return EReplete;
      }

// If we are running a shared file system preform an optional restricted
// pre-selection and then do a standard selection. Since all nodes are equal,
// make sure the client is needlessly avoiding them as this signals an error.
//
   if (baseFS.isDFS())
      {if (Sel.nmask && !(Sel.Opts & XrdCmsSelect::NoTryLim))
          {pmask = (isRW ? pinfo.rwvec : pinfo.rovec) & Sel.nmask;
           if (!(Sel.Opts & XrdCmsSelect::Online))
              pmask |= pinfo.ssvec & Sel.nmask;
           if (pmask && maxBits(pmask, baseFS.dfsTries()))
              {Sel.Resp.DLen = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data)-1,
               "Too many attempts to gain dfs %s access to the file", Amode)+1;
               return RetryErr;
              }
          }
       pmask = amask;
       smask = (Sel.Opts & XrdCmsSelect::Online ? 0 : pinfo.ssvec & amask);
       if (baseFS.Trim())
          {Sel.Resp.DLen = 0;
           if (!(retc = SelDFS(Sel, amask, pmask, smask, isRW)))
              return (fRD ? Cache.WT4File(Sel,Sel.Vec.hf) : Config.LUPDelay);
           if (retc < 0) return retc;
          } else if (noSel) return 0;
       return SelNode(Sel, pmask, smask);
      }

// If either a refresh is wanted or we didn't find the file, re-prime the cache
// which will force the client to wait. Otherwise, compute the primary and
// secondary selections. If there are none, the client may have to wait if we
// have servers that we can query regarding the file. Note that for files being
// opened in write mode, only one writable copy may exist unless this is a
// meta-operation (e.g., remove) in which case the file itself remain unmodified
// or a replica request, in which case we select a new target server.
//
   if (!(Sel.Opts & XrdCmsSelect::Refresh)
   &&   (retc = Cache.GetFile(Sel, pinfo.rovec)))
      {if (isRW)
          {     if (retc<0) return Config.LUPDelay;
              else if (Sel.Opts & XrdCmsSelect::Replica)
                   {pmask = amask & ~(Sel.Vec.hf | Sel.Vec.bf); smask = 0;
                    if (!pmask && !Sel.Vec.bf) return SelFail(Sel,eNoRep);
                   }
           else if (Sel.Vec.bf) pmask = smask = 0;
           else if (Sel.Vec.hf)
                   {if (Sel.Opts & XrdCmsSelect::NewFile) return SelFail(Sel,eExists);
                    if (!(Sel.Opts & XrdCmsSelect::MWFiles))
                       {if (!(Sel.Opts & XrdCmsSelect::isMeta)
                        &&  maxBits(Sel.Vec.hf,2))      return SelFail(Sel,eDups);
                        if ((Sel.Vec.hf & pinfo.rwvec)
                        !=  (Sel.Vec.hf & pinfo.rovec)) return SelFail(Sel,eROfs);
                       }
                    if (!(pmask = Sel.Vec.hf & amask))  return SelFail(Sel,eNoSel);
                    smask = 0;
                   }
           else if (Sel.Opts & (XrdCmsSelect::Trunc | XrdCmsSelect::NewFile))
                   {pmask = amask; smask = 0;}
           else if ((smask = pinfo.ssvec & amask)) pmask = 0;
           else pmask = smask = 0;
          } else {
           pmask = Sel.Vec.hf  & amask; 
           if (Sel.Opts & XrdCmsSelect::Online) {pmask &= ~Sel.Vec.pf; smask=0;}
           else smask = (retc < 0 ? 0 : pinfo.ssvec & amask);
          }
       if (Sel.Vec.hf & Sel.nmask) Cache.UnkFile(Sel, Sel.nmask);
      } else {
       Cache.AddFile(Sel, 0); 
       Sel.Vec.bf = pinfo.rovec; 
       Sel.Vec.hf = Sel.Vec.pf = pmask = smask = 0;
       retc = 0;
      }

// A wait is required if we don't have any primary or seconday servers
//
   dowt = (!pmask && !smask);

// If we can query additional servers, do so now. The client will be placed
// in the callback queue only if we have no possible selections
//
   if (Sel.Vec.bf)
      {CmsStateRequest QReq = {{Sel.Path.Hash, kYR_state, kYR_raw, 0}};
       if (Sel.Opts & XrdCmsSelect::Refresh)
          QReq.Hdr.modifier |= CmsStateRequest::kYR_refresh;
       if (dowt) retc= (fRD ? Cache.WT4File(Sel,Sel.Vec.hf) : Config.LUPDelay);
       TRACE(Files, "seeking " <<Sel.Path.Val);
       amask = Cluster.Broadcast(Sel.Vec.bf, QReq.Hdr,
                                 (void *)Sel.Path.Val,Sel.Path.Len+1);
       if (amask) Cache.UnkFile(Sel, amask);
       if (dowt) return retc;
      } else if (dowt && retc < 0 && !noSel)
                return (fRD ? Cache.WT4File(Sel,Sel.Vec.hf) : Config.LUPDelay);

// Broadcast a freshen up request if wanted
//
   if ((Sel.Opts & XrdCmsSelect::Freshen) && (amask = pmask & ~Sel.Vec.bf))
      {CmsStateRequest Qupt={{0,kYR_state,kYR_raw|CmsStateRequest::kYR_noresp,0}};
       Cluster.Broadcast(amask, Qupt.Hdr,(void *)Sel.Path.Val,Sel.Path.Len+1);
      }

// If we need to defer selection, simply return as this is a mindless prepare
//
   if (noSel) return 0;

// Check if we have no useable servers
//
   if (dowt) return Unuseable(Sel);

// Check if should eliminate staging servers. We may need to do this if the
// client has been eliminating too many of them as they all should be equal.
//
   if (Sel.nmask && pinfo.ssvec && !(Sel.Opts & XrdCmsSelect::NoTryLim)
   &&  maxBits(Sel.nmask & pinfo.ssvec, baseFS.stgTries()))
      {if (!pmask)
          {Sel.Resp.DLen = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data)-1,
           "Too many attempts to stage %s access to the file", Amode)+1;
           return RetryErr;
          }
       smask = 0;
      }

// Select a node
//
   return SelNode(Sel, pmask, smask);
}

/******************************************************************************/
  
int XrdCmsCluster::Select(SMask_t pmask, int &port, char *hbuff, int &hlen,
                          int isrw, int isMulti, int ifWant)
{
   static const SMask_t smLow(255);
   XrdCmsSelector selR;
   XrdCmsNode *nP = 0;
   SMask_t tmask;
   int Snum = 0;
   XrdNetIF::ifType nType = static_cast<XrdNetIF::ifType>(ifWant);

// If there is nothing to select from, return failure
//
   if (!pmask) return 0;

// Obtain the network we need for the client
//
   selR.needNet = XrdNetIF::Mask(nType);

// Initialize
//
   selR.needSpace = 0;

// Packed selection can never occur in this code path so we turn it off
//
   selR.selPack = 0;

// If we are exporting a shared-everything system then the incoming mask
// may have more than one server indicated. So, we need to do a full select.
// This is forced when isMulti is true, indicating a choice may exist. Note
// that the node, if any, is returned unlocked but we have the global mutex.
//
   if (isMulti || baseFS.isDFS())
      {STMutex.ReadLock();
       nP = (Config.sched_RR ? SelbyRef(pmask,selR) : SelbyLoad(pmask,selR));
       if (nP) hlen = nP->netIF.GetName(hbuff, port, nType) + 1;
          else hlen = 0;
       STMutex.UnLock();
       return hlen != 1;
      }

// In shared-nothing systems the incoming mask will only have a single node.
// Compute the a single node number that is contained in the mask.
//
   do {if (!(tmask = pmask & smLow)) Snum += 8;
         else {while((tmask = tmask>>1)) Snum++; break;}
      } while((pmask = pmask >> 8));

// See if the node passes muster
//
   STMutex.ReadLock();
   if ((nP = NodeTab[Snum]))
      {     if (nP->isBad) nP = 0;
       else if (!Config.sched_RR && (nP->myLoad > Config.MaxLoad)) nP = 0;
       else if (!(selR.needNet & nP->hasNet))                      nP = 0;
       if (nP)
          {if (isrw)
              if (nP->isNoStage || nP->DiskFree < nP->DiskMinF)    nP = 0;
                 else {nP->RefTotW++; nP->RefW++;}
              else    {nP->RefTotR++; nP->RefR++;}
          }
      }

// At this point either we have a node or we do not
//
   if (nP)
      {hlen = nP->netIF.GetName(hbuff, port, nType) + 1;
       nP->RefR++;
       STMutex.UnLock();
       return hlen != 1;
      }
   STMutex.UnLock();
   return 0;
}

/******************************************************************************/
/*                               S e l F a i l                                */
/******************************************************************************/
  
int XrdCmsCluster::SelFail(XrdCmsSelect &Sel, int rc)
{
//
    const char *etext, *Item = "file";

    switch(rc)
   {case eExists: if (Sel.Opts & XrdCmsSelect::isMeta) Item = "directory";
                  etext = "Unable to create %s; it already exists.";
                  Sel.Resp.Port = kYR_RWConflict;
                  break;
    case eROfs:   etext = "Unable to modify %s; r/o copy already exists.";
                  Sel.Resp.Port = kYR_RWConflict;
                  break;
    case eDups:   etext = "Unable to modify %s; multiple copies exist.";
                  Sel.Resp.Port = kYR_RWConflict;
                  break;
    case eNoRep:  etext = "Unable to replicate %s; no new sites available.";
                  Sel.Resp.Port = kYR_noReplicas;
                  break;
    case eNoSel:  if (Sel.Vec.hf & Sel.nmask)
                     {etext = "Unable to access %s; eligible servers shunned.";
                      if (Sel.Opts & XrdCmsSelect::isDir) Item = "directory";
                     } else {
                      if (Sel.Opts & XrdCmsSelect::Write)
                         {etext = "Unable to write %s; r/w exports not found.";
                         } else {
                          etext = "Unable to access %s; it does not exist.";
                          if (Sel.Opts & XrdCmsSelect::isDir) Item = "directory";
                         }
                     }
                  Sel.Resp.Port = kYR_ENOENT;
                  break;
    default:      etext = "Unable to access %s; it does not exist.";
                  Sel.Resp.Port = kYR_ENOENT;
                  break;
   };

    int n = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data), etext, Item);
    if (n < (int)sizeof(Sel.Resp.Data)) Sel.Resp.DLen = n+1;
       else Sel.Resp.DLen = sizeof(Sel.Resp.Data);

    return EReplete;
}
  
/******************************************************************************/
/*                                 S p a c e                                  */
/******************************************************************************/
  
void XrdCmsCluster::Space(SpaceData &sData, SMask_t smask)
{
   XrdCmsNode *nP;
   SMask_t bmask;
   int i;
   bool doAll = !baseFS.isDFS();

// Obtain a lock on the table and screen out peer nodes
//
   STMutex.ReadLock();
   bmask = smask & peerMask;

// Run through the table getting space information
//
   for (i = 0; i <= STHi; i++)
       if ((nP = NodeTab[i]) && nP->isNode(bmask) && !(nP->isOffline))
          {if (doAll || !sData.Total) 
              {sData.Total += nP->DiskTotal;
               sData.TotFr += nP->DiskFree;
              }
           if (nP->isRW & XrdCmsNode::allowsSS)
              {sData.sNum++;
               if (sData.sFree < nP->DiskFree)
                  {sData.sFree = nP->DiskFree; sData.sUtil = nP->DiskUtil;}
              }
           if (nP->isRW & XrdCmsNode::allowsRW)
              {sData.wNum++;
               if (sData.wFree < nP->DiskFree)
                  {sData.wFree = nP->DiskFree; sData.wUtil = nP->DiskUtil;
                   sData.wMinF = nP->DiskMinF;
                  }
              }
          }
   STMutex.UnLock();
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdCmsCluster::Stats(char *bfr, int bln)
{
   static const char statfmt1[] = "<stats id=\"cms\">"
                     "<role>%s</role></stats>";
   int mlen;

// Check if actual length wanted
//
   if (!bfr) return  sizeof(statfmt1) + 8;

// Format the statistics (not much here for now)
//
   mlen = snprintf(bfr, bln, statfmt1, Config.myRType);

   if ((bln -= mlen) <= 0) return 0;
   return mlen;
}

/******************************************************************************/
/*                                 S t a t t                                  */
/******************************************************************************/
  
int XrdCmsCluster::Statt(char *bfr, int bln)
{
   static const char statfmt0[] = "</stats>";
   static const char statfmt1[] = "<stats id=\"cmsm\">"
          "<role>%s</role><sel><t>%lld</t><r>%lld</r><w>%lld</w></sel>"
          "<node>%d";
   static const char statfmt2[] = "<stats id=\"%d\">"
          "<host>%s</host><role>%s</role>"
          "<run>%s</run><ref><r>%d</r><w>%d</w></ref>%s</stats>";
   static const char statfmt3[] = "<shr>%d<use>%d</use></shr>";
   static const char statfmt4[] = "</node>";
   static const char statfmt5[] =
          "<frq><add>%lld<d>%lld</d></add><rsp>%lld<m>%lld</m></rsp>"
          "<lf>%lld</lf><ls>%lld</ls><rf>%lld</rf><rs>%lld</rs></frq>";

   static int AddFrq = (Config.RepStats & XrdCmsConfig::RepStat_frq);
   static int AddShr = (Config.RepStats & XrdCmsConfig::RepStat_shr)
                       && Config.asMetaMan();

   XrdCmsRRQ::Info Frq;
   XrdCmsSelected *sp;
   int mlen, tlen, n = 0;
   char shrBuff[80], stat[6], *stp;
   bool oksel;

   class spmngr {
         public: XrdCmsSelected *sp;

                 spmngr() {sp = 0;}
                ~spmngr() {XrdCmsSelected *xsp;
                           while((xsp = sp)) {sp = sp->next; delete xsp;}
                          }
                } mngrsp;

// Check if actual length wanted
//
   if (!bfr)
      {n = sizeof(statfmt0) +
           sizeof(statfmt1) + 12*3 + 3 + 3 +
          (sizeof(statfmt2) + 10*2 + 256 + 16) * STMax + sizeof(statfmt4);
       if (AddShr) n += sizeof(statfmt3) + 12;
       if (AddFrq) n += sizeof(statfmt4) + (10*8);
       return n;
      }

// Get the statistics
//
   if (AddFrq) RRQ.Statistics(Frq);
   mngrsp.sp = sp = List(FULLMASK, LS_NULL, oksel);

// Count number of nodes we have
//
   while(sp) {n++; sp = sp->next;}
   sp = mngrsp.sp;

// Format the statistics
//
   long long lclTcnt = SelTcnt, lclRtot = SelRtot, lclWtot = SelWtot;
   mlen = snprintf(bfr, bln, statfmt1,
          Config.myRType, lclTcnt, lclRtot, lclWtot, n);

   if ((bln -= mlen) <= 0) return 0;
   tlen = mlen; bfr += mlen; n = 0; *shrBuff = 0;

   while(sp && bln > 0)
        {stp = stat;
              if (sp->Status & XrdCmsSelected::Offline) *stp++ = 'o';
         else if (sp->Status & XrdCmsSelected::Suspend) *stp++ = 's';
         else if (sp->Status & XrdCmsSelected::Disable) *stp++ = 'd';
         else *stp++ = 'a';
         if (sp->Status & XrdCmsSelected::isRW)    *stp++ = 'w';
         if (sp->Status & XrdCmsSelected::NoStage) *stp++ = 'n';
         *stp = 0;
         if (AddShr) snprintf(shrBuff, sizeof(shrBuff), statfmt3,
                             (sp->Share ? sp->Share : 100), sp->Shrin);
         mlen = snprintf(bfr, bln, statfmt2, n, sp->Ident,
                XrdCmsRole::Type(static_cast<XrdCmsRole::RoleID>(sp->RoleID)),
                stat, sp->RefTotR, sp->RefTotW, shrBuff);
         bfr += mlen; bln -= mlen; tlen += mlen;
         sp = sp->next; n++;
        }

   if (bln <= (int)sizeof(statfmt4)) return 0;
   strcpy(bfr, statfmt4); mlen = sizeof(statfmt4) - 1;
   bfr += mlen; bln -= mlen; tlen += mlen;

   if (AddFrq && bln > 0)
      {mlen = snprintf(bfr, bln, statfmt5, Frq.Add2Q, Frq.PBack, Frq.Resp,
              Frq.Multi, Frq.luFast, Frq.luSlow, Frq.rdFast, Frq.rdSlow);
       bfr += mlen; bln -= mlen; tlen += mlen;
      }

// See if we overflowed. otherwise finish up
//
   if (sp || bln < (int)sizeof(statfmt0)) return 0;
   strcpy(bfr, statfmt0);
   return tlen + sizeof(statfmt0) - 1;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                             c a l c D e l a y                              */
/******************************************************************************/
  
XrdCmsNode *XrdCmsCluster::calcDelay(XrdCmsSelector &selR)
{
        if (!selR.nPick) {selR.delay = 0;
                          selR.reason = (selR.xNoNet
                                      ? "no eligible servers reachable for"
                                      : "no eligible servers for");
                         }
   else if (selR.xFull)  {selR.delay = Config.DiskWT;
                          selR.reason = "no eligible servers have space for";
                         }
   else if (selR.xOvld)  {selR.delay = Config.MaxDelay;
                          selR.reason = "eligible servers overloaded for";
                         }
   else if (selR.xSusp)  {selR.delay = Config.SUSDelay;
                          selR.reason = "eligible servers suspended for";
                         }
   else if (selR.xOff)   {selR.delay = Config.SUPDelay;
                          selR.reason = "eligible servers offline for";
                         }
   else                  {selR.delay = Config.SUPDelay;
                          selR.reason = "server selection error for";
                         }
   return (XrdCmsNode *)0;
}

/******************************************************************************/
/*                                  D r o p                                   */
/******************************************************************************/
  
// Warning: STMutex must be locked in write upon entry and the caller must
//          release it if this method is called directily. Otherwise, the mutex
//          will be obtained and released. Also, this method may only be called
//          via Remove() either directly or via a deferred job scheduled by that
//          method. This method actually deletes the node object.

int XrdCmsCluster::Drop(int sent, int sinst, XrdCmsDrop *djp)
{
   EPNAME("Drop_Node")
   XrdCmsNode *nP;
   char hname[512];

// If we are being called outside of a scheduled job, obtain the mutex
//
   if (djp) STMutex.WriteLock();

// Make sure this node is the right one
//
   if (!(nP = NodeTab[sent]) || nP->Inst() != sinst)
      {if (nP && djp == nP->DropJob) {nP->DropJob = 0; nP->DropTime = 0;}
       if (djp) STMutex.UnLock();
       DEBUG(sent <<'.' <<sinst <<" cancelled.");
       return 0;
      }

// Check if the drop has been rescheduled
//
   if (djp && time(0) < nP->DropTime)
      {Sched->Schedule((XrdJob *)djp, nP->DropTime);
       if (djp) STMutex.UnLock();
       return 1;
      }

// Save the node name (don't want to hold a lock across a message)
//
   strlcpy(hname, nP->Ident, sizeof(hname));

// Cleanup status
//
   NodeTab[sent] = 0;
   nP->isOffline = 1; // STMutex is locked in write mode
   nP->DropTime  = 0;
   nP->DropJob   = 0;
   nP->isBound   = 0;

// Remove node from the peer list (if it is one)
//
   if (nP->isPeer) {peerHost &= nP->NodeMask; peerMask = ~peerHost;}

// Remove node entry from the alternate list and readjust the end pointer.
//
   if (nP->isMan)
      {memset((void *)&AltMans[sent*AltSize], (int)' ', AltSize);
       if (sent == AltMent)
          {AltMent--;
           while(AltMent >= 0 &&  NodeTab[AltMent]
                              && !NodeTab[AltMent]->isMan) AltMent--;
           if (AltMent < 0) AltMend = AltMans;
              else AltMend = AltMans + ((AltMent+1)*AltSize);
          }
      }

// Readjust STHi
//
   if (sent == STHi) while(STHi >= 0 && !NodeTab[STHi]) STHi--;

// Invalidate any cached entries for this node
//
   if (nP->NodeMask) Cache.Drop(nP->NodeMask, sent, STHi);

// We can now delete the node object if we were called via a job as we are on
// a different thread. Direct calls require that we schedule the deletion as
// it may take a long time if there are oustanding references to this node.
//
   if (djp) {STMutex.UnLock(); nP->Delete(STMutex);}
      else nP->DropJob = new XrdCmsDrop(nP);

// Document the drop
//
   Say.Emsg("Drop_Node", hname, "dropped.");
   return 0;
}

/******************************************************************************/
/*                              M u l t i p l e                               */
/******************************************************************************/

int XrdCmsCluster::Multiple(SMask_t mVec)
{
   static const unsigned long long Left32  = 0xffffffff00000000LL;
   static const unsigned long long Right32 = 0x00000000ffffffffLL;
   static const unsigned long long Left16  = 0x00000000ffff0000LL;
   static const unsigned long long Right16 = 0x000000000000ffffLL;
   static const unsigned long long Left08  = 0x000000000000ff00LL;
   static const unsigned long long Right08 = 0x00000000000000ffLL;
   static const unsigned long long Left04  = 0x00000000000000f0LL;
   static const unsigned long long Right04 = 0x000000000000000fLL;
//                                0 1 2 3 4 5 6 7 8 9 A B C D E F
   static const int isMult[16] = {0,0,0,1,0,1,1,1,0,1,1,1,1,1,1,1};

   if (mVec & Left32) {if (mVec & Right32) return 1;
                          else mVec = mVec >> 32LL;
                      }
   if (mVec & Left16) {if (mVec & Right16) return 1;
                          else mVec = mVec >> 16LL;
                      }
   if (mVec & Left08) {if (mVec & Right08) return 1;
                          else mVec = mVec >>  8LL;
                      }
   if (mVec & Left04) {if (mVec & Right04) return 1;
                          else mVec = mVec >>  4LL;
                      }
   return isMult[mVec];
}
  
/******************************************************************************/
/*                               m a x B i t s                                */
/******************************************************************************/
  
bool XrdCmsCluster::maxBits(SMask_t mVec, int mbits)
{
   int count = 0;

// Count bits. This is the fastest way assuming few bits are set
//
   while(mVec)
        {mVec &= (mVec - 1);
         count++;
         if (count >= mbits) return true;
        }

// Indicate we have not reached the maximum bits set
//
   return false;
}

/******************************************************************************/
/*                                R e c o r d                                 */
/******************************************************************************/
  
void XrdCmsCluster::Record(char *path, const char *reason, bool force)
{
   EPNAME("Record")
   static int msgcnt = 255;
   static XrdSysMutex mcMutex;
   int skipmsg;

   DEBUG(reason <<path);
   mcMutex.Lock();
   msgcnt++; skipmsg = msgcnt & (force ? 0x0f : 0xff);
   mcMutex.UnLock();

   if (!skipmsg) Say.Emsg(epname, "client deferred;", reason, path);
}
 
/******************************************************************************/
/*                               S e l N o d e                                */
/******************************************************************************/
  
int XrdCmsCluster::SelNode(XrdCmsSelect &Sel, SMask_t pmask, SMask_t amask)
{
    EPNAME("SelNode")
    const char *act=0;
    int affsel = 1, count = 0, isalt = 0, pass = 2;
    SMask_t mask;
    XrdCmsNode *nP = 0;
    XrdCmsSelector selR;
    XrdNetIF::ifType nType=(XrdNetIF::ifType)(Sel.Opts & XrdCmsSelect::ifWant);

// Obtain the network we need for the client
//
   selR.needNet = XrdNetIF::Mask(nType);

// Indicate whether or not stable selection is required
//
   if (!(Sel.Opts & XrdCmsSelect::Pack)) selR.selPack = 0;
      else {unsigned int theHash = (Sel.Opts & XrdCmsSelect::UseAH
                                 ?  Sel.AltHash : Sel.Path.Hash);
            SMask_t sVec = pmask;
            for (count = 0; sVec; count++) sVec &= (sVec - 1);
            if (count > 1) selR.selPack = affsel = (theHash % count) + 1;
               else        selR.selPack = 0;
           }

// There is a difference bwteen needing space and needing r/w access. The former
// is needed when we will be writing data the latter for inode modifications.
//
   if (Sel.Opts & XrdCmsSelect::isMeta) selR.needSpace = 0;
      else selR.needSpace = (Sel.Opts & XrdCmsSelect::Write
                          ?  XrdCmsNode::allowsRW : 0);

// Scan for a primary and alternate node (alternates do staging). At this
// point we omit all peer nodes as they are our last resort. Note that Selbyxxx
// returns the node unlocked but we have the global mutex so that is OK.
//
   STMutex.ReadLock();
   mask = pmask & peerMask;
   while(pass--)
        {if (mask)
            {nP = (Config.sched_RR || (Sel.Opts & XrdCmsSelect::UseRef)
                ?  SelbyRef(mask,selR) : SelbyLoad(mask,selR));
             if (nP || (selR.nPick && selR.delay)
             ||  NodeCnt < Config.SUPCount) break;
            }
         mask = amask & peerMask; isalt = XrdCmsNode::allowsSS;
         if (!(Sel.Opts & XrdCmsSelect::isMeta)) selR.needSpace |= isalt;
        }

// Produce affinity result trace
//
   if (Sel.Opts & XrdCmsSelect::Pack && nP)
      {TRACE(Redirect, "affinity " <<affsel <<'/' <<count <<'/'
                       <<(int)selR.selPack <<(selR.selPack ? " go " : " ng ")
                       <<nP->Name() <<' ' <<Sel.Path.Val);
      }

// If we found an eligible node then dispatch the client to it. We will
// swap the global mutex for the node mutex to minimize interefrence.
//
   if (nP)
      {nP->g2nLock(STMutex);
       Sel.Resp.DLen = nP->netIF.GetName(Sel.Resp.Data, Sel.Resp.Port, nType);
       if (!Sel.Resp.DLen) {nP->UnLock(); return Unreachable(Sel, false);}
       Sel.Resp.DLen++; Sel.smask = nP->NodeMask;

       // If a message is to be sent to the selected server, send it.
       //
       if (Sel.iovN && Sel.iovP) nP->Send(Sel.iovP, Sel.iovN);

       // Do special post proccessing when any of:
       // a) isalt true: Secondary selection occurred
       // b) Create set: File creation will occur
       //
       if (isalt || (Sel.Opts & XrdCmsSelect::Create))
          {Sel.Opts |= (XrdCmsSelect::Pending | XrdCmsSelect::Advisory);
           if (Sel.Opts & XrdCmsSelect::noBind) act = " handling ";
              else Cache.AddFile(Sel, nP->NodeMask);
          }

       // Determine what we are actually doing here
       //
       nP->UnLock();
       if (!act)
          {if (isalt) act = (Sel.iovN ? " staging " : " assigned ");
              else    act = " serving ";
          }
       TRACE(Stage, Sel.Resp.Data <<act <<Sel.Path.Val);
       return 0;
      }

// No node so check if we have a sufficient number to continue. Note that we
// do not forward to a peer unless we have a suffficient number of local nodes.
//
   if (!selR.delay && NodeCnt < Config.SUPCount)
      {STMutex.UnLock();
       Record(Sel.Path.Val, "insufficient number of nodes", true);
       return Config.SUPDelay;
      }

// Return delay if we should avoid selecting a peer manager
//
   if (selR.delay && selR.delay < Config.PSDelay)
      {STMutex.UnLock();
       Record(Sel.Path.Val, selR.reason);
       return selR.delay;
      }

// At this point, we attempt a peer node selection (choice of last resort). Note
// that we are still holding the global lock! If we find a peer node we will
// swap it with the node lock.
//
   if (Sel.Opts & XrdCmsSelect::Peers)
      {const char *reason1 = selR.reason;
       int delay1 = selR.delay;
       bool noNet = selR.xNoNet;
       if ((mask = (pmask | amask) & peerHost)) nP = SelbyCost(mask, selR);
       if (nP)
          {nP->g2nLock(STMutex);
           Sel.Resp.DLen = nP->netIF.GetName(Sel.Resp.Data,Sel.Resp.Port,nType);
           if (!Sel.Resp.DLen) {nP->UnLock(); return Unreachable(Sel, false);}
           Sel.Resp.DLen++; Sel.smask = nP->NodeMask;
           if (Sel.iovN && Sel.iovP) nP->Send(Sel.iovP, Sel.iovN);
           nP->UnLock();
           TRACE(Stage, "Peer " <<Sel.Resp.Data <<" handling " <<Sel.Path.Val);
           return 0;
          }
       if (!selR.delay)
          {selR.delay = delay1; selR.reason = reason1; selR.xNoNet = noNet;}
      }

// At this point we don't need the global lock so let it go.
//
   STMutex.UnLock();

// At this point we either don't have enough nodes or simply can't handle this
//
   if (selR.delay)
      {Record(Sel.Path.Val, selR.reason);
       return selR.delay;
      }

// Return appropriate error message
//
   if (selR.xNoNet) return Unreachable(Sel, true);
   return Unuseable(Sel);
}

/******************************************************************************/
/*                              R e f C o u n t                               */
/******************************************************************************/

// This snippet of code occurrs often enough so that we make it a macro as we
// want to execute this inline.
//
#define RefCount(sP, sPMulti, NeedSpace)                       \
        if (NeedSpace) {sP->RefTotW++; sP->RefW++;} \
           else        {sP->RefTotR++; sP->RefR++;} \
        if (sPMulti && sP->Share && !sP->Shrem--)              \
           {sP->RefW += sP->Shrip; sP->RefR += sP->Shrip;      \
            sP->Shrem = sP->Share; sP->Shrin++;                \
           }
  
/******************************************************************************/
/*                             S e l b y C o s t                              */
/******************************************************************************/

// Cost selection is used only for peer node selection as peers do not
// report a load and handle their own scheduling.

// Caller must have the STMutex locked. The returned node, if any, is unlocked.

XrdCmsNode *XrdCmsCluster::SelbyCost(SMask_t mask, XrdCmsSelector &selR)
{
    XrdCmsNode *np, *sp = 0;
    bool Multi = false;

// Scan for a node (sp points to the selected one)
//
   selR.Reset(); SelTcnt++;
   for (int i = 0; i <= STHi; i++)
       if ((np = NodeTab[i]) && (np->NodeMask & mask))
          {if (!(selR.needNet &  np->hasNet))    {selR.xNoNet= true; continue;}
           selR.nPick++;
           if (np->isOffline)                    {selR.xOff  = true; continue;}
           if (np->isBad)                        {selR.xSusp = true; continue;}
           if (selR.needSpace && np->isNoStage)  {selR.xFull = true; continue;}
           if (!sp) sp = np;
              else{if (abs(sp->myCost - np->myCost) <= Config.P_fuzz)
                      {     if (selR.selPack)
                               {if (--selR.selPack) sp=np;
                                   else break;
                               }
                       else if (selR.needSpace)
                               {if (sp->RefW > (np->RefW+Config.DiskLinger))
                                   sp=np;
                               }
                       else if (sp->RefR > np->RefR) sp=np;
                      }
                       else if (sp->myCost > np->myCost) sp=np;
                   Multi = true;
                  }
          }

// Check for overloaded node and return result
//
   if (!sp) return calcDelay(selR);
   RefCount(sp, Multi, selR.needSpace);
   return sp;
}
  
/******************************************************************************/
/*                             S e l b y L o a d                              */
/******************************************************************************/

// Caller must have the STMutex locked. The returned node, if any, is unlocked.
  
XrdCmsNode *XrdCmsCluster::SelbyLoad(SMask_t mask, XrdCmsSelector &selR)
{
    XrdCmsNode *np, *sp = 0;
    bool Multi = false, reqSS = (selR.needSpace & XrdCmsNode::allowsSS) != 0;

// Scan for a node (preset possible, suspended, overloaded, full, and dead)
//
   selR.Reset(); SelTcnt++;
   for (int i = 0; i <= STHi; i++)
       if ((np = NodeTab[i]) && (np->NodeMask & mask))
          {if (!(selR.needNet & np->hasNet))      {selR.xNoNet= true; continue;}
           selR.nPick++;
           if (np->isOffline)                     {selR.xOff  = true; continue;}
           if (np->isBad)                         {selR.xSusp = true; continue;}
           if (np->myLoad > Config.MaxLoad)       {selR.xOvld = true; continue;}
           if (selR.needSpace && (np->DiskFree < np->DiskMinF
                                  || (reqSS && np->isNoStage)))
              {selR.xFull = true; continue;}
           if (!sp) sp = np;
              else{if (selR.needSpace)
                      {if (abs(sp->myMass - np->myMass) <= Config.P_fuzz)
                          {if (sp->RefW > (np->RefW+Config.DiskLinger)) sp=np;}
                          else if (sp->myMass > np->myMass)             sp=np;
                      } else {
                       if (abs(sp->myLoad - np->myLoad) <= Config.P_fuzz)
                          {if (selR.selPack)
                              {if (--selR.selPack)                      sp=np;
                                  else break;
                              }
                              else if (sp->RefR > np->RefR)             sp=np;
                          }
                          else if (sp->myLoad > np->myLoad)             sp=np;
                      }
                   Multi = true;
                  }
          }

// Check for overloaded node and return result
//
   if (!sp) return calcDelay(selR);
   RefCount(sp, Multi, selR.needSpace);
   return sp;
}

/******************************************************************************/
/*                              S e l b y R e f                               */
/******************************************************************************/

// Caller must have the STMutex locked. The returned node, if any, is unlocked.

XrdCmsNode *XrdCmsCluster::SelbyRef(SMask_t mask, XrdCmsSelector &selR)
{
    XrdCmsNode *np, *sp = 0;
    bool Multi = false, reqSS = (selR.needSpace & XrdCmsNode::allowsSS) != 0;

// Scan for a node (sp points to the selected one)
//
   selR.Reset(); SelTcnt++;
   for (int i = 0; i <= STHi; i++)
       if ((np = NodeTab[i]) && (np->NodeMask & mask))
          {if (!(selR.needNet & np->hasNet))    {selR.xNoNet= true; continue;}
           selR.nPick++;
           if (np->isOffline)                   {selR.xOff  = true; continue;}
           if (np->isBad)                       {selR.xSusp = true; continue;}
           if (selR.needSpace && (np->DiskFree < np->DiskMinF
                                  || (reqSS && np->isNoStage)))
              {selR.xFull = true; continue;}
           if (!sp) sp = np;
              else {Multi = true;
                         if (selR.selPack)
                            {if (--selR.selPack) sp=np;
                                else break;
                            }
                    else if (selR.needSpace)
                            {if (sp->RefW > (np->RefW+Config.DiskLinger)) sp=np;}
                    else if (sp->RefR > np->RefR)                         sp=np;
                   }
          }

// Check for overloaded node and return result
//
   if (!sp) return calcDelay(selR);
   RefCount(sp, Multi, selR.needSpace);
   return sp;
}
 
/******************************************************************************/
/*                                S e l D F S                                 */
/******************************************************************************/
  
int XrdCmsCluster::SelDFS(XrdCmsSelect &Sel, SMask_t amask,
                          SMask_t &pmask, SMask_t &smask, int isRW)
{
   EPNAME("SelDFS");
   static const SMask_t allNodes(~0);
   int oldOpts, rc;

// The first task is to find out if the file exists somewhere. If we are doing
// local queries, then the answer will be immediate. Otherwise, forward it.
//
   if ((Sel.Opts & XrdCmsSelect::Refresh) || !(rc = Cache.GetFile(Sel, amask)))
      {if (!baseFS.Local())
          {CmsStateRequest QReq = {{Sel.Path.Hash, kYR_state, kYR_raw, 0}};
           TRACE(Files, "seeking " <<Sel.Path.Val);
           Cache.AddFile(Sel, 0);
           if (Sel.Opts & XrdCmsSelect::Refresh)
              QReq.Hdr.modifier |= CmsStateRequest::kYR_refresh;
           Cluster.Broadsend(amask, QReq.Hdr, Sel.Path.Val, Sel.Path.Len+1);
           return 0;
          }
       if ((rc = baseFS.Exists(Sel.Path.Val, -Sel.Path.Len)) < 0)
          {Cache.AddFile(Sel, 0);
           Sel.Vec.bf = Sel.Vec.pf = Sel.Vec.wf = Sel.Vec.hf = 0;
          } else {
           Sel.Vec.hf = amask; Sel.Vec.wf = (isRW ? amask : 0);
           oldOpts = Sel.Opts;
           if (rc != CmsHaveRequest::Pending) Sel.Vec.pf = 0;
              else {Sel.Vec.pf = amask; Sel.Opts |= XrdCmsSelect::Pending;}
           Cache.AddFile(Sel, allNodes);
           Sel.Opts = oldOpts;
          }
      }

// Screen out online requests where the file is pending
//
   if (Sel.Opts & XrdCmsSelect::Online && Sel.Vec.pf)
      {pmask = smask = 0;
       return 1;
      }

// If the file is to be written and the files exists then it can't be a new file
//
   if (isRW && Sel.Vec.hf)
      {if (Sel.Opts & XrdCmsSelect::NewFile) return SelFail(Sel,eExists);
       if (Sel.Opts & XrdCmsSelect::Trunc) smask = 0;
       return 1;
      }

// Final verification that we have something to select
//
   if (!Sel.Vec.hf
   && (!isRW || !(Sel.Opts & (XrdCmsSelect::Trunc | XrdCmsSelect::NewFile))))
      return SelFail(Sel, eNoEnt);
   return 1;
}
  
/******************************************************************************/
/*                             s e n d A L i s t                              */
/******************************************************************************/
  
// Single entry at a time, protected by STMutex in write mode!

void XrdCmsCluster::sendAList(XrdLink *lp)
{
   static CmsTryRequest Req = {{0, kYR_try, 0, 0}, 0};
   static int HdrSize = sizeof(Req.Hdr) + sizeof(Req.sLen);
   static char *AltNext = AltMans;
   static struct iovec iov[4] = {{(caddr_t)&Req, (size_t)HdrSize},
                                 {0, 0},
                                 {AltMans, 0},
                                 {(caddr_t)"\0", 1}};
   int dlen;

// Calculate what to send
//
   AltNext = AltNext + AltSize;
   if (AltNext >= AltMend)
      {AltNext = AltMans;
       iov[1].iov_len = 0;
       iov[2].iov_len = dlen = AltMend - AltMans;
      } else {
        iov[1].iov_base = (caddr_t)AltNext;
        iov[1].iov_len  = AltMend - AltNext;
        iov[2].iov_len  = AltNext - AltMans;
        dlen = iov[1].iov_len + iov[2].iov_len;
      }

// Complete the request (account for trailing null character)
//
   dlen++;
   Req.Hdr.datalen = htons(static_cast<unsigned short>(dlen+sizeof(Req.sLen)));
   Req.sLen = htons(static_cast<unsigned short>(dlen));

// Send the list of alternates (rotated once)
//
   lp->Send(iov, 4, dlen+HdrSize);
}

/******************************************************************************/
/*                             s e t A l t M a n                              */
/******************************************************************************/
  
// Single entry at a time, protected by STMutex in write mode!
  
void XrdCmsCluster::setAltMan(int snum, XrdLink *lp, int port)
{
   XrdNetAddr altAddr = *(lp->NetAddr());
   char *ap = &AltMans[snum*AltSize];
   int i;

// Preset the buffer and pre-screen the port number
//
   if (!port || (port > 0x0000ffff)) port = Config.PortTCP;
   memset(ap, int(' '), AltSize);

// First tr to use the hostname:port which may be too large (unlikely). Else
// Insert the ip address of this node into the list of nodes. We made sure that
// the size of he buffer was big enough so no need to check for overflow.
//
   altAddr.Port(port);
   if (Config.DoHnTry) i = altAddr.Format(ap, AltSize, XrdNetAddr::fmtName);
      else i = 0;
   if (!i) i=altAddr.Format(ap,AltSize,XrdNetAddr::fmtAddr,XrdNetAddr::prefipv4);
   ap[i] = ' ';

// Compute new fence
//
   if (ap >= AltMend) {AltMend = ap + AltSize; AltMent = snum;}
}

/******************************************************************************/
/*                           U n r e a c h a b l e                            */
/******************************************************************************/
  
int XrdCmsCluster::Unreachable(XrdCmsSelect &Sel, bool none)
{
   XrdNetIF::ifType nType=(XrdNetIF::ifType)(Sel.Opts & XrdCmsSelect::ifWant);
   const char *Amode = (Sel.Opts & XrdCmsSelect::Write  ? "write" : "read");
   const char *Xmode = (Sel.Opts & XrdCmsSelect::Online ? "immediately " : "");

   if (none)
      {Sel.Resp.DLen = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data)-1,
               "No servers are reachable via %s network to %s%s the file.",
               XrdNetIF::Name(nType), Xmode, Amode) + 1;
      } else {
       Sel.Resp.DLen = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data)-1,
               "Eligible server is unreachable via %s network to %s%s the file.",
               XrdNetIF::Name(nType), Xmode, Amode) + 1;
      }
   Sel.Resp.Port = kYR_ENETUNREACH;
   return EReplete;
}
  
/******************************************************************************/
/*                             U n u s e a b l e                              */
/******************************************************************************/
  
int XrdCmsCluster::Unuseable(XrdCmsSelect &Sel)
{
   const char *Amode = (Sel.Opts & XrdCmsSelect::Write  ? "write" : "read");
   const char *Xmode = (Sel.Opts & XrdCmsSelect::Online ? "immediately " : "");
   const char *EType = (Sel.Opts & XrdCmsSelect::isDir  ? "directory" : "file");

   int n = snprintf(Sel.Resp.Data, sizeof(Sel.Resp.Data),
                   "No servers are available to %s%s the %s.",
                   Xmode, Amode, EType);
    if (n < (int)sizeof(Sel.Resp.Data)) Sel.Resp.DLen = n+1;
       else Sel.Resp.DLen = sizeof(Sel.Resp.Data);

   Sel.Resp.Port = kYR_ENOENT;
   return EReplete;
}

/******************************************************************************/
/*                                                                            */
/*                      X r d C m s M a n a g e r . c c                       */
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

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdCms/XrdCmsConfig.hh"
#include "XrdCms/XrdCmsManager.hh"
#include "XrdCms/XrdCmsManTree.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsProtocol.hh"
#include "XrdCms/XrdCmsRouting.hh"
#include "XrdCms/XrdCmsUtils.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTokenizer.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdCms
{
extern XrdSysError     Say;

extern XrdSysTrace     Trace;
}

using namespace XrdCms;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysMutex   XrdCmsManager::MTMutex;
XrdCmsNode   *XrdCmsManager::MastTab[MTMax] = {0};
char          XrdCmsManager::MastSID[MTMax] = {0};
int           XrdCmsManager::MTHi = -1;
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdCmsDelNode : XrdJob
{
public:

     void DoIt() {nodeP->Delete(XrdCmsManager::MTMutex);
                  delete this;
                 }

          XrdCmsDelNode(XrdCmsNode *nP) : XrdJob("delete node"), nodeP(nP)
                       {Sched->Schedule((XrdJob *)this);}

         ~XrdCmsDelNode() {}

XrdCmsNode *nodeP;
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCmsManager::XrdCmsManager(XrdOucTList *mlP, int snum)
{
     myMans    = 0;
     ManTree   = 0;
     curManCnt = 0;
     curManList= mlP;
     newManList= 0;
     theSite   = 0;
     theHost   = 0;
     theSID    = 0;
     siteID    = snum;
     wasRedir  = false;
}

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
XrdCmsNode *XrdCmsManager::Add(XrdLink *lp, int Lvl, bool &xit)
{
   EPNAME("Add")
   XrdCmsNode *nP;
   int i;

// Check if there is a pending reconfiguration. If so, return no node but
// tell the caller to finish so we can proceed with the reconfiguration
//
   MTMutex.Lock();
   lp->setID("manager",0);
   if (newManList) {MTMutex.UnLock(); xit = true; return 0;}
   xit = false;

// Find available ID for this node
//
   for (i = 0; i < MTMax; i++) if (!MastTab[i]) break;

// Check if we have too many here
//
   if (i >= MTMax)
      {MTMutex.UnLock();
       Say.Emsg("Manager", "Login to", lp->Name(), "failed; too many managers");
       return 0;
      }

// Obtain a new a new node object
//
   if (!(nP = new XrdCmsNode(lp, 0, 0, 0, Lvl, i)))
        {Say.Emsg("Manager", "Unable to obtain node object."); return 0;}

// Assign new manager
//
   MastTab[i] = nP;
   MastSID[i] = siteID;
   if (i > MTHi) MTHi = i;
   nP->isOffline  = 0;
   nP->isNoStage  = 0;
   nP->isBad      = 0;
   nP->isBound    = 1;
   nP->isConn     = 1;
   nP->isMan      = (Config.asManager() ? 1 : 0);
   nP->setManager(this);
   MTMutex.UnLock();

// Document login
//
   DEBUG(nP->Name() <<" to manager config; id=" <<i);
   return nP;
}

/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/
  
void XrdCmsManager::Delete(XrdCmsNode *nodeP)
{
     new XrdCmsDelNode(nodeP);
}

/******************************************************************************/
/*                              F i n i s h e d                               */
/******************************************************************************/
  
void XrdCmsManager::Finished(const char *manP, int mPort)
{
   XrdOucTList *mP;
   char mbuff[16];

// Indicate what we are disbanding
//
   sprintf(mbuff, ":%d", mPort);
   Say.Say("Config ", "Manager ", manP, mbuff, " unconfigured.");

// Serialize
//
   MTMutex.Lock();

// If this is this is the last manager connection and we have a pending new
// list of managers, run those now. We waited so as to not overwhelm the system.
//
   curManCnt--;
   if (curManCnt > 0 || !newManList) {MTMutex.UnLock(); return;}

// Remove all vestigial information
//
   for (int i = 0; i <= MTHi; i++)
       {if (MastSID[i] == siteID) {MastTab[i] = 0; MastSID[i] = 0;}}

// Readjust the high water mark
//
   while(MTHi >= 0 && !MastTab[MTHi]) MTHi--;

// Delete the current manager list, it is safe to do so
//
   while((mP = curManList)) {curManList = curManList->next; delete mP;}
   curManList = newManList;
   newManList = 0;

// Run the new manager setup
//
   Say.Say("Config ","Manager subsystem reconfiguration completed; restarting.");
   Run(curManList);

// All done
//
   MTMutex.UnLock();
}
  
/******************************************************************************/
/*                                I n f o r m                                 */
/******************************************************************************/
  
void XrdCmsManager::Inform(const char *What, const char *Data, int Dlen)
{
   EPNAME("Inform");
   XrdCmsNode *nP;
   int i;

// Obtain a lock on the table
//
   MTMutex.Lock();

// Run through the table looking for managers to send messages to
//
   for (i = 0; i <= MTHi; i++)
       {if ((nP=MastTab[i]) && !nP->isOffline)
           {nP->Lock();
            MTMutex.UnLock();
            DEBUG(nP->Name() <<" " <<What);
            nP->Send(Data, Dlen);
            nP->UnLock();
            MTMutex.Lock();
           }
       }
   MTMutex.UnLock();
}

/******************************************************************************/
  
void XrdCmsManager::Inform(const char *What, struct iovec *vP, int vN, int vT)
{
   EPNAME("Inform");
   int i;
   XrdCmsNode *nP;

// Obtain a lock on the table
//
   MTMutex.Lock();

// Run through the table looking for managers to send messages to
//
   for (i = 0; i <= MTHi; i++)
       {if ((nP=MastTab[i]) && !nP->isOffline)
           {nP->Lock();
            MTMutex.UnLock();
            DEBUG(nP->Name() <<" " <<What);
            nP->Send(vP, vN, vT);
            nP->UnLock();
            MTMutex.Lock();
           }
       }
   MTMutex.UnLock();
}
  
/******************************************************************************/

void XrdCmsManager::Inform(XrdCms::CmsReqCode rCode, int rMod,
                                  const char *Arg,  int Alen)
{
  CmsRRHdr Hdr = {0, (kXR_char)rCode, (kXR_char)rMod,
                    htons(static_cast<unsigned short>(Alen))};
    struct iovec ioV[2] = {{(char *)&Hdr, sizeof(Hdr)},
                           {(char *)Arg, (size_t)Alen}};

    Inform(Router.getName((int)rCode), ioV, (Arg ? 2 : 1), Alen+sizeof(Hdr));
}

/******************************************************************************/

void XrdCmsManager::Inform(CmsRRHdr &Hdr, const char *Arg, int Alen)
{
    struct iovec ioV[2] = {{(char *)&Hdr, sizeof(Hdr)},
                           {(char *)Arg, (size_t)Alen}};

    Hdr.datalen = htons(static_cast<unsigned short>(Alen));

    Inform(Router.getName(Hdr.rrCode), ioV, (Arg ? 2 : 1), Alen+sizeof(Hdr));
}

/******************************************************************************/
/*                                R e m o v e                                 */
/******************************************************************************/

void XrdCmsManager::Remove(XrdCmsNode *nP, const char *reason)
{
   EPNAME("Remove")
   int sinst, sent = nP->ID(sinst);

// Obtain a lock on the servtab
//
   MTMutex.Lock();

// Make sure this node is the right one
//
   if (!(nP == MastTab[sent]))
      {MTMutex.UnLock();
       DEBUG("manager " <<sent <<'.' <<sinst <<" failed.");
       return;
      }

// Remove node from the manager table
//
   MastTab[sent] = 0;
   MastSID[sent] = 0;
   nP->isOffline = 1;
   DEBUG("completed " <<nP->Name() <<" manager " <<sent <<'.' <<sinst);

// Readjust MTHi
//
   if (sent == MTHi) while(MTHi >= 0 && !MastTab[MTHi]) MTHi--;
   MTMutex.UnLock();

// Document removal
//                                                                             .
   if (reason) Say.Emsg("Manager", nP->Ident, "removed;", reason);
}

/******************************************************************************/
/*                                 R e r u n                                  */
/******************************************************************************/
  
void XrdCmsManager::Rerun(char *newMans)
{
   static CmsDiscRequest discRequest = {{0, kYR_disc, 0, 0}};
   XrdOucTList *tP;
   const char *eText;
   char *hP;
   int newManCnt = 0;

// Lock ourselves
//
   MTMutex.Lock();
   wasRedir = true;

// If we already have a pending new sequence, then just return
//
   if (newManList) {MTMutex.UnLock(); return;}

// Indicate that we will be re-initialzing
//
   Say.Say("Config ", "Manager subsystem reconfiguring using ", newMans);

// Process the new man list
//
   XrdNetAddr manAddr;
   XrdOucTokenizer mList((char *)newMans);
   hP = mList.GetLine();

// Add each manager in the list. These have already been expanded and are
// gaurenteed to contain a port number as the list is provided by the cmsd.
// However, we will check for duplicates and ignore any overage.
//
   while((hP = mList.GetToken()))
        {if ((eText = manAddr.Set(hP)))
            {Say.Emsg("Config","Ignoring manager", hP, eText); continue;}
         tP = newManList;
         while(tP && strcmp(hP, tP->text)) tP = tP->next;
         if (tP) {Say.Emsg("Config","Ignoring duplicate manager", hP);
                  continue;
                 }
         if (newManCnt >=MTMax)
            {Say.Emsg("Config","Ignoring manager", hP,
                               "and remaining entries; limit exceeded!");
             break;
            }
         newManList = new XrdOucTList(manAddr.Name(),manAddr.Port(),newManList);
         newManCnt++;
        }

// If we have managers then tell the cluster builder to abort as we will
// be restarting this whole process (we don't want any hung nodes here).
//
   if (newManCnt) ManTree->Abort();

// Now run through the node table and doom all current site connections as we
// need to reinitialize the whole manager subsystem. Note that none of these
// objects can escape without us removing them from the table.
//
   if (newManCnt)
      {for (int i = 0; i <= MTHi; i++)
           if (MastTab[i] && (MastSID[i] == siteID))
              {MastTab[i]->isBad |= XrdCmsNode::isBlisted|XrdCmsNode::isDoomed;
               MastTab[i]->Send((char *)&discRequest, sizeof(discRequest));
              }
      }

// We are done
//
   MTMutex.UnLock();
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdCmsManager::Reset()
{
   EPNAME("Reset");
   static CmsStatusRequest myState = {{0, kYR_status, 
                                       CmsStatusRequest::kYR_Reset, 0}};
   static const int        szReqst = sizeof(CmsStatusRequest);
   XrdCmsNode *nP;
   int i;

// Obtain a lock on the table
//
   MTMutex.Lock();

// Run through the table looking for managers to send a reset request
//
   for (i = 0; i <= MTHi; i++)
       {if ((nP=MastTab[i]) && !nP->isOffline && nP->isKnown)
           {nP->Lock();
            nP->isKnown = 0;
            MTMutex.UnLock();
            DEBUG("sent to " <<nP->Name());
            nP->Send((char *)&myState, szReqst);
            nP->UnLock();
            MTMutex.Lock();
           }
       }
   MTMutex.UnLock();
}

/******************************************************************************/
/* Private:                          R u n                                    */
/******************************************************************************/
  
int XrdCmsManager::Run(XrdOucTList *manL)
{
   XrdOucTList   *tP = manL;
   XrdJob        *jP, *jFirst = 0, *jLast = 0;

// This method is either called during initial start-up or if we were wholly
// redirected elsewhere due to a blacklist. In the latter case, the caller
// must have obtained all the required locks
//
   curManCnt = 0;
   if (!manL) return 0;

// Prime the manager subsystem. We check here to make sure we will not be
// tying to connect to ourselves. This is possible if the manager and meta-
// manager were defined to be the same and we are a manager. We would have
// liked to screen this out earlier but port discovery prevents it.
//
   while(tP)
        {if (strcmp(tP->text, Config.myName) || tP->val != Config.PortTCP)
            {jP = (XrdJob *)XrdCmsProtocol::Alloc(Config.myRole, this,
                                                  tP->text, tP->val);
             if (!jFirst) jFirst = jLast = jP;
                else {jLast->NextJob = jP; jLast = jP;}
             curManCnt++;
            } else {
              char buff[512];
              sprintf(buff, "%s:%d", tP->text, tP->val);
              Say.Emsg("Config", "Circular connection to", buff, "ignored.");
            }
         tP = tP->next;
        }

// Make sure we have something to start up
//
   if (!curManCnt)
      {Say.Emsg("Config","No managers can be started; we are now unreachable!");
       return 0;
      }

// We now know there is no pandering going on, so we need to initialize the
// the tree management subsystem to get it into a fresh state.
//
   if (myMans)  delete myMans;
   myMans  = new XrdCmsManList;
   if (ManTree) delete ManTree;
   ManTree = new XrdCmsManTree(curManCnt);
   if (theSID)  {free(theSID);  theSID  = 0;}
   if (theSite) {free(theSite); theSite = 0;}

// Now start up all of the threads
//
   if (jFirst == jLast) Sched->Schedule(jFirst);
      else Sched->Schedule(curManCnt, jFirst, jLast);

// All done
//
   return curManCnt;
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
bool XrdCmsManager::Start(const XrdOucTList *mL)
{
   XrdOucTList *manVec[MTMax] = {0};
   XrdCmsManager *manP;
   char buff[1024];
   int n, sid, snum = 0, mtot = 0, mnum = 0, xnum = 0;

// If there is no manager list then we must not be connecting to anyone
//
   if (!mL) return true;
  
// Segregate the manager list by site and run them that way. Unfortunately,
// that means we have to copy the TList. This ok as this happens once.
//
   while(mL)
        {sid = mL->ival[1]; mtot++;
         if (sid >= MTMax)
            {sprintf(buff, "%d", sid);
             Say.Say("Config ", "Invalid site ID ", buff, " for ", mL->text);
            } else {
             manVec[sid] = new XrdOucTList(mL->text, mL->val, manVec[sid]);
             mnum++;
            }
          mL = mL->next;
        }

// Count how many sites we have
//
   for (n = 0; n < MTMax; n++) if (manVec[n]) snum++;

// Indicate what we are about to do
//
   snprintf(buff, sizeof(buff),"%d manager%s and %d site%s.", mnum,
                              (mnum != 1 ? "s":""), snum, (snum != 1 ? "s":""));
   Say.Say("Config Connecting to ", buff);

// Now run each one
//
   for (n = 0; n < MTMax; n++)
       {if (manVec[n])
           {manP = new XrdCmsManager(manVec[n], n);
            xnum += manP->Run(manVec[n]);
           }
       }

// Check if we should issue a warning
//
   if (xnum < mtot)
      {snprintf(buff, sizeof(buff), "%d of %d", xnum, mtot);
       Say.Say("Config Warning! Only ", buff, " manager(s) will be contacted!");
      }

// All done
//
   return xnum == mtot;
}

/******************************************************************************/
/*                                V e r i f y                                 */
/******************************************************************************/

bool XrdCmsManager::Verify(XrdLink *lP, const char *sid, const char *sname)
{
   XrdSysMutexHelper hMutex(MTMutex);
   const char *sidP;

// Trim off the type of service in the sid
//
   if ((sidP = index(sid, ' '))) sidP++;
      else sidP = sid;

// If we have no sid, just record it
//
   if (!theSID)
      {theSID = strdup(sidP);
       if (theSite) free(theSite);
       theHost = strdup(lP->Host());
       theSite = (sname ? strdup(sname) : strdup("anonymous"));
       return true;
      }

// Make sure we are connecting to the same cluster as before
//
   if (!strcmp(theSID, sidP)) return true;

// Compute the offending site configuration
//
   char mBuff[1024];
   snprintf(mBuff,sizeof(mBuff),"%s for site %s; "
            "making file location unpredictable!", theHost,
            (wasRedir ? theSite : XrdCmsUtils::SiteName(siteID)));

// There seems to be a configuration error here
//
   Say.Emsg("Manager", lP->Host(), "manager configuration differs from", mBuff);
   return false;
}

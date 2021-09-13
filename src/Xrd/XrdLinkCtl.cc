/******************************************************************************/
/*                                                                            */
/*                         X r d L i n k C t l . c c                          */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <sys/types.h>
#include <fcntl.h>
#include <ctime>

#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLinkCtl.hh"
#include "Xrd/XrdLinkMatch.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdScheduler.hh"

#define  TRACELINK this
#include "Xrd/XrdTrace.hh"

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdGlobal
{
extern XrdSysError  Log;
extern XrdScheduler Sched;
extern XrdInet     *XrdNetTCP;
};

using namespace XrdGlobal;
  
/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/

       XrdLinkCtl    **XrdLinkCtl::LinkTab  = 0;
       char           *XrdLinkCtl::LinkBat  = 0;
       unsigned int    XrdLinkCtl::LinkAlloc= 0;
       int             XrdLinkCtl::LTLast   = -1;
       int             XrdLinkCtl::maxFD    = 0;
       XrdSysMutex     XrdLinkCtl::LTMutex;
       short           XrdLinkCtl::killWait = 3;  // Kill then wait;
       short           XrdLinkCtl::waitKill = 4;  // Wait then kill

       const char     *XrdLinkCtl::TraceID = "LinkCtl";

namespace
{
       XrdSysMutex     instMutex;
       unsigned int    myInstance = 1;
       int             idleCheck;
       int             idleTicks;

static const int       XRDLINK_USED = 0x01;
static const int       XRDLINK_FREE = 0x00;

class LinkScan : public XrdJob
{
public:

void  DoIt() {XrdLinkCtl::idleScan();
              Sched.Schedule((XrdJob *)this, idleCheck+time(0));
             }
      LinkScan() : XrdJob("Idle link scan") {}
     ~LinkScan() {}
};
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdLink *XrdLinkCtl::Alloc(XrdNetAddr &peer, int opts)
{
   XrdLinkCtl *lp;
   char hName[1024], *unp, buff[32];
   int bl, peerFD = peer.SockFD();

// Make sure that the incoming file descriptor can be handled
//
   if (peerFD < 0 || peerFD >= maxFD)
      {snprintf(hName, sizeof(hName), "%d", peerFD);
       Log.Emsg("Link", "attempt to alloc out of range FD -",hName);
       return (XrdLink *)0;
      }

// Make sure that the link slot is available
//
   LTMutex.Lock();
   if (LinkBat[peerFD])
      {LTMutex.UnLock();
       Log.Emsg("Link", "attempt to reuse active link");
       return (XrdLink *)0;
      }

// Check if we already have a link object in this slot. If not, allocate
// a quantum of link objects and put them in the table.
//
   if (!(lp = LinkTab[peerFD]))
      {unsigned int i;
       XrdLinkCtl **blp, *nlp = new XrdLinkCtl[LinkAlloc]();
       if (!nlp)
          {LTMutex.UnLock();
           Log.Emsg("Link", ENOMEM, "create link");
           return (XrdLink *)0;
          }
       blp = &LinkTab[peerFD/LinkAlloc*LinkAlloc];
       for (i = 0; i < LinkAlloc; i++, blp++) *blp = &nlp[i];
       lp = LinkTab[peerFD];
      }
      else lp->Reset();
   LinkBat[peerFD] = XRDLINK_USED;
   if (peerFD > LTLast) LTLast = peerFD;
   LTMutex.UnLock();

// Establish the instance number of this link. This is will prevent us from
// sending asynchronous responses to the wrong client when the file descriptor
// gets reused for connections to the same host.
//
   instMutex.Lock();
   lp->Instance = myInstance++;
   instMutex.UnLock();

// Establish the address and connection name of this link
//
   peer.Format(hName, sizeof(hName), XrdNetAddr::fmtAuto,
                                     XrdNetAddr::old6Map4 | XrdNetAddr::noPort);
   lp->HostName = strdup(hName);
   lp->HNlen = strlen(hName);
   XrdNetTCP->Trim(hName);
   lp->Addr = peer;
   strlcpy(lp->Lname, hName, sizeof(lp->Lname));
   bl = sprintf(buff, "anon.0:%d", peerFD);
   unp = lp->Uname + sizeof(Uname) - bl - 1; // Solaris compatibility
   memcpy(unp, buff, bl);
   lp->ID = unp;
   lp->PollInfo.FD = lp->LinkInfo.FD = peerFD;
   lp->Comment = (const char *)unp;

// Set options as needed
//
   lp->LockReads = (0 != (opts & XRDLINK_RDLOCK));
   lp->KeepFD    = (0 != (opts & XRDLINK_NOCLOSE));

// Update statistics and return the link. We need to actually get the stats
// mutex even when using atomics because we need to use compound operations.
// The atomics will keep reporters from seeing partial results.
//
   statsMutex.Lock();
   AtomicInc(LinkCountTot);            // LinkCountTot++
   if (LinkCountMax <= AtomicInc(LinkCount)) LinkCountMax = LinkCount;
   statsMutex.UnLock();
   return lp;
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/

// Warning: curr must be set to a value of 0 or less on the initial call and
//          not touched therafter unless a null pointer is returned. When an
//          actual link object pointer is returned, it's refcount is increased.
//          The count is automatically decreased on the next call to Find().
//
XrdLink *XrdLinkCtl::Find(int &curr, XrdLinkMatch *who)
{
   XrdLinkCtl *lp;
   const int MaxSeek = 16;
   unsigned int myINS;
   int i, seeklim = MaxSeek;

// Do initialization
//
   LTMutex.Lock();
   if (curr >= 0 && LinkTab[curr]) LinkTab[curr]->setRef(-1);
      else curr = -1;

// Find next matching link. Since this may take some time, we periodically
// release the LTMutex lock which drives up overhead but will still allow
// other critical operations to occur.
//
   for (i = curr+1; i <= LTLast; i++)
       {if ((lp = LinkTab[i]) && LinkBat[i] && lp->HostName)
           if (!who 
           ||   who->Match(lp->ID,lp->Lname-lp->ID-1,lp->HostName,lp->HNlen))
              {myINS = lp->Instance;
               LTMutex.UnLock();
               lp->setRef(1);
               curr = i;
               if (myINS == lp->Instance) return lp;
               LTMutex.Lock();
              }
        if (!seeklim--) {LTMutex.UnLock(); seeklim = MaxSeek; LTMutex.Lock();}
       }

// Done scanning the table
//
    LTMutex.UnLock();
    curr = -1;
    return 0;
}
  
/******************************************************************************/
/*                               g e t N a m e                                */
/******************************************************************************/

// Warning: curr must be set to a value of 0 or less on the initial call and
//          not touched therafter unless null is returned. Returns the length
//          the name in nbuf.
//
int XrdLinkCtl::getName(int &curr, char *nbuf, int nbsz, XrdLinkMatch *who)
{
   XrdLinkCtl *lp;
   const int MaxSeek = 16;
   int i, ulen = 0, seeklim = MaxSeek;

// Find next matching link. Since this may take some time, we periodically
// release the LTMutex lock which drives up overhead but will still allow
// other critical operations to occur.
//
   LTMutex.Lock();
   for (i = curr+1; i <= LTLast; i++)
       {if ((lp = LinkTab[i]) && LinkBat[i] && lp->HostName)
           if (!who 
           ||   who->Match(lp->ID,lp->Lname-lp->ID-1,lp->HostName,lp->HNlen))
              {ulen = lp->Client(nbuf, nbsz);
               LTMutex.UnLock();
               curr = i;
               return ulen;
              }
        if (!seeklim--) {LTMutex.UnLock(); seeklim = MaxSeek; LTMutex.Lock();}
       }
   LTMutex.UnLock();

// Done scanning the table
//
   curr = -1;
   return 0;
}

/******************************************************************************/
/*                              i d l e S c a n                               */
/******************************************************************************/
  
#undef   TRACELINK
#define  TRACELINK lp

void XrdLinkCtl::idleScan()
{
   XrdLinkCtl *lp;
   int i, ltlast, lnum = 0, tmo = 0, tmod = 0;

// Get the current link high watermark
//
   LTMutex.Lock();
   ltlast = LTLast;
   LTMutex.UnLock();

// Scan across all links looking for idle links. Links are never deallocated
// so we don't need any special kind of lock for these
//
   for (i = 0; i <= ltlast; i++)
       {if (LinkBat[i] != XRDLINK_USED
        || !(lp = LinkTab[i])) continue;
        lnum++;
        lp->LinkInfo.opMutex.Lock();
        if (lp->isIdle) tmo++;
        lp->isIdle++;
        if ((int(lp->isIdle)) < idleTicks)
           {lp->LinkInfo.opMutex.UnLock(); continue;}
        lp->isIdle = 0;
        if (!(lp->PollInfo.Poller) || !(lp->PollInfo.isEnabled))
           Log.Emsg("LinkScan","Link",lp->ID,"is disabled and idle.");
           else if (lp->LinkInfo.InUse == 1)
                   {lp->PollInfo.Poller->Disable(lp->PollInfo, "idle timeout");
                    tmod++;
                   }
        lp->LinkInfo.opMutex.UnLock();
       }

// Trace what we did
//
   TRACE(CONN, lnum <<" links; " <<tmo <<" idle; " <<tmod <<" force closed");
}

/******************************************************************************/
/*                                s e t K W T                                 */
/******************************************************************************/
  
void XrdLinkCtl::setKWT(int wkSec, int kwSec)
{
   if (wkSec > 0) waitKill = static_cast<short>(wkSec);
   if (kwSec > 0) killWait = static_cast<short>(kwSec);
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/

int XrdLinkCtl::Setup(int maxfds, int idlewait)
{
   int numalloc;

// Compute the number of link objects we should allocate at a time. Generally,
// we like to allocate 8k of them at a time but always as a power of two.
//
   maxFD = maxfds;
   numalloc = 8192 / sizeof(XrdLink);
   LinkAlloc = 1;
   while((numalloc = numalloc/2)) LinkAlloc = LinkAlloc*2;
   TRACE(DEBUG, "Allocating " <<LinkAlloc <<" link objects at a time");

// Create the link table
//
   if (!(LinkTab = (XrdLinkCtl **)malloc(maxfds*sizeof(XrdLinkCtl*)+LinkAlloc)))
      {Log.Emsg("Link", ENOMEM, "create LinkTab"); return 0;}
   memset((void *)LinkTab, 0, maxfds*sizeof(XrdLinkCtl *));

// Create the slot status table
//
   if (!(LinkBat = (char *)malloc(maxfds*sizeof(char)+LinkAlloc)))
      {Log.Emsg("Link", ENOMEM, "create LinkBat"); return 0;}
   memset((void *)LinkBat, XRDLINK_FREE, maxfds*sizeof(char));

// Create an idle connection scan job
//
   if (idlewait)
      {if ((idleCheck = idlewait/3)) idleTicks = 3;
          else {idleTicks = 1;
                idleCheck = idlewait;
               }
       LinkScan *ls = new LinkScan;
       Sched.Schedule((XrdJob *)ls, idleCheck+time(0));
      }

// All done
//
   return 1;
}

/******************************************************************************/
/*                               S y n c A l l                                */
/******************************************************************************/
  
void XrdLinkCtl::SyncAll()
{
   int myLTLast;

// Get the current last entry
//
   LTMutex.Lock(); myLTLast = LTLast; LTMutex.UnLock();

// Run through all the links and sync the statistics
//
   for (int i = 0; i <= myLTLast; i++)
       {if (LinkBat[i] == XRDLINK_USED && LinkTab[i]) LinkTab[i]->syncStats();}
}

/******************************************************************************/
/*                                U n h o o k                                 */
/******************************************************************************/

void XrdLinkCtl::Unhook(int fd)
{

// Indicate link no longer actvely neing used
//
   LTMutex.Lock();
   LinkBat[fd] = XRDLINK_FREE;
   if (fd == LTLast) while(LTLast && !(LinkBat[LTLast])) LTLast--;
   LTMutex.UnLock();
}

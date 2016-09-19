/******************************************************************************/
/*                                                                            */
/*                      X r d C m s M a n L i s t . c c                       */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include "XrdCms/XrdCmsManList.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdCmsManRef
{
public:

XrdCmsManRef *Next;
char         *Manager;
unsigned int  ManRef;
int           ManPort;
int           ManLvl;

              XrdCmsManRef(unsigned int ref, char *name, int port, int lvl)
                          : Next(0), Manager(name), ManRef(ref),
                            ManPort(port), ManLvl(lvl) {};

             ~XrdCmsManRef() {if (Manager) free(Manager);}
};

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdCmsManList::~XrdCmsManList()
{
   XrdCmsManRef *prp, *mrp = allMans;

   while(mrp) {prp = mrp; mrp = mrp->Next; delete prp;}
}


/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
void XrdCmsManList::Add(const XrdNetAddr *netAddr, char *redList,
                        int manPort, int lvl)
{
   XrdOucTokenizer hList((char *)redList);
   char *hP;
   int theRef;

// Get the manager's reference number and if exists, delete existing entries
//
   if ((theRef = getRef(netAddr)) >= 0) Del(theRef);
      else theRef = -theRef;

// Add eeach redirect target in the list
//
   hP = hList.GetLine();
   while((hP = hList.GetToken())) Add(theRef, hP, manPort, lvl);
}

/******************************************************************************/
  
void XrdCmsManList::Add(int ref, char *manp, int manport, int lvl)
{
   XrdCmsManRef *prp = 0, *mrp;
   char *cp, *ipname;
   int port;

// Find the colon in the host name
//
   if (*manp != '[') cp = index(manp, int(':'));
      else if ((cp = index(manp+1, ']'))) cp = index(cp+1, int(':'));
   if (!cp) port = manport;
      else {if (!(port=atoi(cp+1)) || port > 0xffff) port=manport;
            *cp = '\0';
           }

// Get the full name of the host target unless it is an actual address
//
   if (XrdNetAddrInfo::isHostName(manp)) ipname = strdup(manp);
      else {XrdNetAddr  redAddr;
            const char *redName;
            if (redAddr.Set(manp,0) || !(redName = redAddr.Name())) return;
            ipname = strdup(redName);
           }
   if (cp) *cp = ':';

// Start up
//
   mlMutex.Lock();
   mrp = allMans;

// Chck if this is a duplicate
//
   while(mrp)
        {if (!strcmp(mrp->Manager, ipname) && mrp->ManPort == port)
            {mlMutex.UnLock(); free(ipname); return;}
         if (mrp->Next)
            {if (mrp->Next->ManLvl > lvl) prp = mrp;}
            else if (!prp) prp = mrp;
         mrp = mrp->Next;
        }

// Create new entry
//
   mrp = new XrdCmsManRef(ref, ipname, port, lvl);
   if (!prp) nextMan = allMans = mrp;
      else {mrp->Next = prp->Next; prp->Next = mrp;
            if (nextMan->ManLvl > lvl) nextMan = mrp;
           }
   mlMutex.UnLock();
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdCmsManList::Del(int ref)
{
   XrdCmsManRef *nrp, *prp = 0, *mrp;

// If mistakingly called for a newly added reference, do nothing
//
   if (ref < 0) return;

// Start up
//
   mlMutex.Lock();
   mrp = allMans;

// Delete all ref entries
//
   while(mrp)
        {if (mrp->ManRef == (unsigned int)ref)
            {nrp = mrp->Next;
             if (!prp) allMans  = nrp;
                else {prp->Next = nrp;
                      if (mrp == allMans) allMans = nrp;
                     }
             if (mrp == nextMan) nextMan = nrp;
             delete mrp;
             mrp = nrp;
            } else {prp = mrp; mrp = mrp->Next;}
        }

// All done
//
   mlMutex.UnLock();
}

/******************************************************************************/
/*                                g e t R e f                                 */
/******************************************************************************/
  
int XrdCmsManList::getRef(const XrdNetAddr *netAddr)
{
   static int refNum = 1;
   XrdNetAddr theAddr = *netAddr;
   XrdOucTList *tP;
   char buff[128];
   int theNum;

// Convert address to text
//
   theAddr.Format(buff,sizeof(buff),XrdNetAddr::fmtAdv6,XrdNetAddr::old6Map4);

// Find the entry in this list
//
   refMutex.Lock();
   tP = refList;
   while(tP && strcmp(buff, tP->text)) tP = tP->next;

// If we didn't find one, add it
//
   if (tP) theNum = tP->val;
      else {refList = new XrdOucTList(buff, refNum, refList);
            theNum = -refNum++;
           }

// Return the number
//
   refMutex.UnLock();
   return theNum;
}

/******************************************************************************/
/*                                  N e x t                                   */
/******************************************************************************/
  
int XrdCmsManList::Next(int &port, char *buff, int bsz)
{
   XrdCmsManRef *np;
   int lvl;

   mlMutex.Lock();
   if (!(np = nextMan)) nextMan = allMans;
      else {strlcpy(buff, np->Manager, bsz);
            port = np->ManPort;
            nextMan = np->Next;
           }
   lvl = (np ? np->ManLvl : 0);
   mlMutex.UnLock();
   return lvl;
}

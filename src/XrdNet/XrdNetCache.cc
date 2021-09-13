/******************************************************************************/
/*                                                                            */
/*                        X r d N e t C a c h e . c c                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <ctime>
#include <sys/socket.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdNet/XrdNetCache.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
int XrdNetCache::keepTime = 0;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetCache::XrdNetCache(int psize, int csize)
{
     prevtablesize = psize;
     nashtablesize = csize;
     Threshold     = (csize * LoadMax) / 100;
     nashnum       = 0;
     nashtable     = (anItem **)malloc( (size_t)(csize*sizeof(anItem *)) );
     memset((void *)nashtable, 0, (size_t)(csize*sizeof(anItem *)));
}

/******************************************************************************/
/* public                            A d d                                    */
/******************************************************************************/
  
void XrdNetCache::Add(XrdNetAddrInfo *hAddr, const char *hName)
{
   anItem Item, *hip;
   int    kent;

// Get the key and make sure this is a valid address (should be)
//
   if (!GenKey(Item, hAddr)) return;

// We may be in a race condition, check we have this item
//
   myMutex.Lock();
   if ((hip = Locate(Item)))
      {if (hip->hName) free(hip->hName);
       hip->hName = strdup(hName);
       hip->expTime = time(0) + keepTime;
       myMutex.UnLock();
       return;
      }

// Check if we should expand the table
//
   if (++nashnum > Threshold) Expand();

// Allocate a new entry
//
   hip = new anItem(Item, hName, keepTime);

// Add the entry to the table
//
   kent = hip->aHash % nashtablesize;
   hip->Next = nashtable[kent];
   nashtable[kent] = hip;
   myMutex.UnLock();
}
  
/******************************************************************************/
/* private                        E x p a n d                                 */
/******************************************************************************/
  
void XrdNetCache::Expand()
{
   int newsize, newent, i;
   size_t memlen;
   anItem **newtab, *nip, *nextnip;

// Compute new size for table using a fibonacci series
//
   newsize = prevtablesize + nashtablesize;

// Allocate the new table
//
   memlen = (size_t)(newsize*sizeof(anItem *));
   if (!(newtab = (anItem **) malloc(memlen))) return;
   memset((void *)newtab, 0, memlen);

// Redistribute all of the current items
//
   for (i = 0; i < nashtablesize; i++)
       {nip = nashtable[i];
        while(nip)
             {nextnip = nip->Next;
              newent  = nip->aHash % newsize;
              nip->Next = newtab[newent];
              newtab[newent] = nip;
              nip = nextnip;
             }
       }

// Free the old table and plug in the new table
//
   free((void *)nashtable);
   nashtable     = newtab;
   prevtablesize = nashtablesize;
   nashtablesize = newsize;

// Compute new expansion threshold
//
   Threshold = static_cast<int>((static_cast<long long>(newsize)*LoadMax)/100);
}

/******************************************************************************/
/* public                           F i n d                                   */
/******************************************************************************/
  
char *XrdNetCache::Find(XrdNetAddrInfo *hAddr)
{
  anItem Item, *nip, *pip = 0;
  int kent;

// Get the hash for this address
//
   if (!GenKey(Item, hAddr)) return 0;

// Compute position of the hash table entry
//
   myMutex.Lock();
   kent = Item.aHash%nashtablesize;

// Find the entry
//
   nip = nashtable[kent];
   while(nip && *nip != Item) {pip = nip; nip = nip->Next;}
   if (!nip) {myMutex.UnLock(); return 0;}

// Make sure entry has not expired
//
   if (nip->expTime > time(0))
      {char *hName = strdup(nip->hName);
       myMutex.UnLock();
       return hName;
      }

// Remove the entry and return not found
//
   if (pip) pip->Next       = nip->Next;
      else  nashtable[kent] = nip->Next;
   myMutex.UnLock();
   delete nip;
   return 0;
}

/******************************************************************************/
/*                                G e n K e y                                 */
/******************************************************************************/
  
int XrdNetCache::GenKey(XrdNetCache::anItem &Item, XrdNetAddrInfo *hAddr)
{
   union aPoint
        {const sockaddr     *sAddr;
         const sockaddr_in  *sAddr4;
         const sockaddr_in6 *sAddr6;
        } aP;
   aP.sAddr = hAddr->SockAddr();
   union{long long llVal; int intVal[2];} Temp;
   int family = hAddr->Family();

// Get the size, validate, and generate the key
//
   if (family == AF_INET)
      {memcpy(Item.aVal, &(aP.sAddr4->sin_addr), 4);
       Item.aHash = Item.aV4[0];
       Item.aLen  = 4;
       return 1;
      }

   if (family == AF_INET6)
      {memcpy(Item.aVal, &(aP.sAddr6->sin6_addr), 16);
       Temp.llVal = Item.aV6[0]    ^ Item.aV6[1];
       Item.aHash = Temp.intVal[0] ^ Temp.intVal[1];
       Item.aLen  = 16;
       return 1;
      }

   return 0;
}

/******************************************************************************/
/* Private:                       L o c a t e                                 */
/******************************************************************************/
  
XrdNetCache::anItem *XrdNetCache::Locate(XrdNetCache::anItem &Item)
{
  anItem *nip;
  unsigned int kent;

// Find the entry
//
   kent = Item.aHash%nashtablesize;
   nip = nashtable[kent];
   while(nip && *nip != Item) nip = nip->Next;
   return nip;
}

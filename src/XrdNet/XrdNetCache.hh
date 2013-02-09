#ifndef __XRDNETCACHE_HH__
#define __XRDNETCACHE_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d N e t C a c h e . h h                         */
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

#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "XrdSys/XrdSysPthread.hh"

class XrdNetAddrInfo;
  
class XrdNetCache
{
public:

//------------------------------------------------------------------------------
//! Add an address-hostname association to the cache. The address must be an
//! INET family address; otherwise it is not added.
//!
//! @param  hAddr  points to the address of the name.
//! @param  hName  points to the name to be associated with the address.
//------------------------------------------------------------------------------

void   Add(XrdNetAddrInfo *hAddr, const char *hName);

//------------------------------------------------------------------------------
//! Locate an address-hostname association in the cache.
//!
//! @param  hAddr  points to the address of the name.
//!
//! @return Success: an strdup'd string of the corresponding name.
//!         Failure: 0;
//------------------------------------------------------------------------------

char  *Find(XrdNetAddrInfo *hAddr);

//------------------------------------------------------------------------------
//! Set the default keep time for entries in the cache during initialization.
//!
//! @param  ktVal  the number of seconds to keep an entry in the cache.
//------------------------------------------------------------------------------
static
void   SetKT(int ktval) {keepTime = ktval;}

//------------------------------------------------------------------------------
//! Constructor. When allocateing a new hash, two adjacent Fibonocci numbers.
//! The series is simply n[j] = n[j-1] + n[j-2].
//!
//! @param  psize  the correct Fibonocci antecedent to csize.
//! @param  csize  the initial size of the table.
//------------------------------------------------------------------------------

       XrdNetCache(int psize = 987, int csize = 1597);

//------------------------------------------------------------------------------
//! Destructor. The XrdNetCache object is not designed to be deleted. Doing
//! so will cause memory to be lost.
//------------------------------------------------------------------------------

      ~XrdNetCache() {} // Never gets deleted

private:

static const int LoadMax = 80;

struct anItem
      {union    {long long aV6[2];
                 int       aV4[4];
                 char      aVal[16];  // Enough for IPV4 or IPV6
                };
       anItem   *Next;
       char     *hName;
       time_t    expTime;   // Expiration time
unsigned int     aHash;     // Hash value
       int       aLen;      // Actual length 4 or 16

inline int       operator!=(const anItem &oth)
                           {return aLen != oth.aLen || aHash != oth.aHash
                                || memcmp(aVal, oth.aVal, aLen);
                           }

                 anItem() : Next(0), hName(0), aLen(0) {}

                 anItem(anItem &Item, const char *hn, int kt)
                         : Next(0), hName(strdup(hn)), expTime(time(0)+kt),
                           aHash(Item.aHash), aLen(Item.aLen)
                         {memcpy(aVal, Item.aVal, Item.aLen);}
                ~anItem() {if (hName) free(hName);}
      };

void             Expand();
int              GenKey(anItem &Item, XrdNetAddrInfo *hAddr);
anItem          *Locate(anItem &Item);

static int       keepTime;

XrdSysMutex      myMutex;
anItem         **nashtable;
int              prevtablesize;
int              nashtablesize;
int              nashnum;
int              Threshold;
};
#endif

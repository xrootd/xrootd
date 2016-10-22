#ifndef __XRDCMSKEY_HH__
#define __XRDCMSKEY_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d C m s K e y . h h                           */
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

#include <string.h>

#include "XrdCms/XrdCmsTypes.hh"

/******************************************************************************/
/*                       C l a s s   X r d C m s K e y                        */
/******************************************************************************/
  
// The XrdCmsKey object describes a key (in our case a path). It is used to
// locate cached keys and is updated with relevant information as it is
// processed in order to speed up the search when multiple lookups occur.
//
class XrdCmsKeyItem;

class XrdCmsKey
{
public:

XrdCmsKeyItem    *TODRef;
char             *Val;
unsigned int      Hash;
short             Len;
unsigned char     TOD;
unsigned char     Ref;

void              setHash();

inline int        Equiv(XrdCmsKey &oth)
                       {return Hash == oth.Hash && Ref == oth.Ref;}

inline XrdCmsKey& operator=(const XrdCmsKey &rhs)
                           {Val = strdup(rhs.Val); Hash = rhs.Hash; 
                            Len = rhs.Len;
                            return *this;
                           }

inline int        operator==(const XrdCmsKey &oth)
                          {return Hash == oth.Hash && !strcmp(Val, oth.Val);}

inline int        operator!=(const XrdCmsKey &oth)
                          {return Hash != oth.Hash || strcmp(Val, oth.Val);}

         XrdCmsKey(char *key=0, int klen=0)
                      : TODRef(0), Val(key), Hash(0), Len(klen), Ref('\0') {}
        ~XrdCmsKey() {};
};

/******************************************************************************/
/*                    C l a s s   X r d C m s K e y L o c                     */
/******************************************************************************/
  
// The XrdCmsKeyLoc object describes the location of the key (servers as well
// our local cache). The semantics differ depending on whether it is in the
// cache or the information has been reported out of the cache.
//
class XrdCmsKeyLoc
{
public:

SMask_t        hfvec;    // Servers that are staging or have the file
SMask_t        pfvec;    // Servers that are staging         the file
SMask_t        qfvec;    // Servers that are not yet queried
unsigned int   TOD_B;    // Server currency clock
int            lifeline; // TOD when nil entry should expire
union {
unsigned int   HashSave; // Where hash goes upon item unload
int            deadline;
      };
short          roPend;   // Redirectors waiting for R/O response
short          rwPend;   // Redirectors waiting for R/W response

inline 
XrdCmsKeyLoc&  operator=(const XrdCmsKeyLoc &rhs)
                           {hfvec=rhs.hfvec; pfvec=rhs.pfvec; TOD_B=rhs.TOD_B;
                            deadline = rhs.deadline;
                            lifeline = rhs.lifeline;
                            roPend = rhs.roPend; rwPend = rhs.rwPend;
                            return *this;
                           }

               XrdCmsKeyLoc() : lifeline(0), roPend(0), rwPend(0) {}
              ~XrdCmsKeyLoc() {}
};
  
/******************************************************************************/
/*                   C l a s s   X r d C m s K e y I t e m                    */
/******************************************************************************/
  
// The XrdCmsKeyItem object marries the XrdCmsKey and XrdCmsKeyLoc objects in
// the key cache. It is only used by logical manipulator, XrdCmsCache, which
// always front-ends the physical manipulator, XrdCmsNash.
//
class XrdCmsKeyItem
{
public:

       XrdCmsKeyLoc   Loc;
       XrdCmsKey      Key;
       XrdCmsKeyItem *Next;

static XrdCmsKeyItem *Alloc(unsigned int theTock);

       void           Recycle();

       void           Reload();

static int            Replenish();

static void           Stats(int &isAlloc, int &isFree, int &wasEmpty);

static XrdCmsKeyItem *Unload(unsigned int   theTock);

static XrdCmsKeyItem *Unload(XrdCmsKeyItem *theItem);

       XrdCmsKeyItem() {}  // Warning see the constructor!
      ~XrdCmsKeyItem() {}  // These are usually never deleted

static const unsigned int TickRate =   64;
static const unsigned int TickMask =   63;
static const          int minAlloc = 4096;
static const          int minFree  = 1024;

private:

static XrdCmsKeyItem *TockTable[TickRate];
static XrdCmsKeyItem *Free;
static int            numFree;
static int            numHave;
static int            numNull;
};
#endif

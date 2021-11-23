#ifndef __XRDFRCXATTR_HH__
#define __XRDFRCXATTR_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d F r c X A t t r . h h                         */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>
#include <cinttypes>
#include <netinet/in.h>
#include <sys/types.h>
#include <cstring>

#include "XrdSys/XrdSysPlatform.hh"

/* XrdFrcXAttr encapsulates the extended attributes needed to determine
   file residency. It is used by the FRM in migrate and purge processing as well
   as for the OSS to determine file residency in memory. It is self-contained
   to prevent circular dependencies.
*/

/******************************************************************************/
/*                        X r d F r c X A t t r C p y                         */
/******************************************************************************/
  
class XrdFrcXAttrCpy
{
public:

long long cpyTime;     // Last time file was copied
char      Rsvd[16];    // Reserved fields

/* postGet() will put cpyTime in host byte order (see preSet()).
*/
       int             postGet(int Result)
                              {if (Result > 0) cpyTime = ntohll(cpyTime);
                               return Result;
                              }

/* preSet() will put cpyTime in network byte order to allow the attribute to
            to be copied to different architectures and still work.
*/
       XrdFrcXAttrCpy *preSet(XrdFrcXAttrCpy &tmp)
                             {tmp.cpyTime = htonll(cpyTime); return &tmp;}

/* Name() returns the extended attribute name for this object.
*/
static const char     *Name() {return "XrdFrm.Cpy";}

/* sizeGet() and sizeSet() return the actual size of the object is used.
*/
static int             sizeGet() {return sizeof(XrdFrcXAttrCpy);}
static int             sizeSet() {return sizeof(XrdFrcXAttrCpy);}

       XrdFrcXAttrCpy() : cpyTime(0) {memset(Rsvd, 0, sizeof(Rsvd));}
      ~XrdFrcXAttrCpy() {}
};
  
/******************************************************************************/
/*                        X r d F r c X A t t r M e m                         */
/******************************************************************************/
  
class XrdFrcXAttrMem
{
public:

char      Flags;       // See definitions below
char      Rsvd[7];     // Reserved fields

// The following flags are defined for Flags
//
static const char memMap  = 0x01; // Mmap the file
static const char memKeep = 0x02; // Mmap the file and keep mapping
static const char memLock = 0x04; // Mmap the file and lock it in memory

/* postGet() and preSet() are minimal as no chages are needed
*/
static int             postGet(int Result)         {return Result;}
       XrdFrcXAttrMem *preSet(XrdFrcXAttrMem &tmp) {return this;}

/* Name() returns the extended attribute name for this object.
*/
static const char     *Name() {return "XrdFrm.Mem";}

/* sizeGet() and sizeSet() return the actual size of the object is used.
*/
static int             sizeGet() {return sizeof(XrdFrcXAttrMem);}
static int             sizeSet() {return sizeof(XrdFrcXAttrMem);}

       XrdFrcXAttrMem() : Flags(0) {memset(Rsvd, 0, sizeof(Rsvd));}
      ~XrdFrcXAttrMem() {}
};

/******************************************************************************/
/*                        X r d F r c X A t t r P i n                         */
/******************************************************************************/
  
class XrdFrcXAttrPin
{
public:

long long pinTime;     // Pin-to-time or pin-for-time value
char      Flags;       // See definitions below
char      Rsvd[7];     // Reserved fields

// The following flags are defined for Flags
//
static const char pinPerm = 0x01; // Pin forever
static const char pinIdle = 0x02; // Pin unless pinTime idle met
static const char pinKeep = 0x04; // Pin until  pinTime
static const char pinSet  = 0x07; // Pin is valid

/* postGet() will put pinTime in host byte order (see preSet()).
*/
       int             postGet(int Result)
                              {if (Result > 0) pinTime = ntohll(pinTime);
                               return Result;
                              }

/* preSet() will put pinTime in network byte order to allow the attribute to
            to be copied to different architectures and still work.
*/
       XrdFrcXAttrPin *preSet(XrdFrcXAttrPin &tmp)
                             {tmp.pinTime = htonll(pinTime); tmp.Flags = Flags;
                              return &tmp;
                             }

/* Name() returns the extended attribute name for this object.
*/
static const char     *Name() {return "XrdFrm.Pin";}


/* sizeGet() and sizeSet() return the actual size of the object is used.
*/
static int             sizeGet() {return sizeof(XrdFrcXAttrCpy);}
static int             sizeSet() {return sizeof(XrdFrcXAttrCpy);}

       XrdFrcXAttrPin() : pinTime(0), Flags(0) {memset(Rsvd, 0, sizeof(Rsvd));}
      ~XrdFrcXAttrPin() {}
};

/******************************************************************************/
/*                        X r d F r c X A t t r P f n                         */
/******************************************************************************/
  
class XrdFrcXAttrPfn
{
public:

char      Pfn[MAXPATHLEN+8]; // Enough room for the Pfn

/* postGet() and preSet() are minimal as no chages are needed
*/
static int             postGet(int Result)         {return Result;}
       XrdFrcXAttrPfn *preSet(XrdFrcXAttrPfn &tmp) {return this;}

/* Name() returns the extended attribute name for this object.
*/
static const char     *Name() {return "XrdFrm.Pfn";}

/* sizeGet() return the actual size of the object is used.
*/
static int             sizeGet() {return sizeof(XrdFrcXAttrPfn);}

/* sizeSet() returns the length of the Pfn string plus the null byte.
*/
       int             sizeSet() {return strlen(Pfn)+1;}

       XrdFrcXAttrPfn() {*Pfn = 0;}
      ~XrdFrcXAttrPfn() {}
};
#endif

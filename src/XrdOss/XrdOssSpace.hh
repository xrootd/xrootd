#ifndef _OSS_SPACE_H
#define _OSS_SPACE_H
/******************************************************************************/
/*                                                                            */
/*                        X r d O s s S p a c e . h h                         */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdSysError;

class XrdOssSpace
{
public:
friend class XrdOssCache;

       enum       sType {Serv = 0, Pstg = 1, Purg = 2, Admin = 3,
                         RsvA = 4, RsvB = 5, RsvC = 6, addT  = 7,
                         Totn = 8};

static const int  maxSNlen = 63;  // Maximum space name length (+1 for null)
static const int  minSNbsz = 64;

static void       Adjust(int Gent,          off_t Space, sType=Serv);

static void       Adjust(const char *GName, off_t Space, sType=Serv);

static const int  haveUsage = 1;
static const int  haveQuota = 2;

static int        Init(); // Return the "or" of havexxxx (above)

static int        Init(const char *aPath,const char *qFile,int isSOL,int us=0);

static int        Quotas();

static int        Unassign(const char *GName);

static long long  Usage(int gent);

struct uEnt {char      gName[minSNbsz];
             long long Bytes[Totn]; // One of sType, above
            };

static long long  Usage(const char *GName, struct uEnt &uVal, int rrd=0);

                  XrdOssSpace() {}  // Everything is static
                 ~XrdOssSpace() {}  // Never gets deleted

private:

static int    Assign(const char *GName, long long &bytesUsed);
static int    findEnt(const char *GName);
static int    Readjust();
static int    Readjust(int);
static int    UsageLock(int Dolock=1);

static const int ULen   = sizeof(long long);
static const int DataSz = 16384;
static const int maxEnt = DataSz/sizeof(uEnt);

static const char *qFname;
static const char *uFname;
static const char *uUname;
static uEnt        uData[maxEnt];
static short       uDvec[maxEnt];
static time_t      lastMtime;
static time_t      lastUtime;
static int         fencEnt;
static int         freeEnt;
static int         aFD;
static int         uSync;
static int         uAdj;
static int         Solitary;
};
#endif

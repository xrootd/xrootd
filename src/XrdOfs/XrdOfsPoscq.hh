#ifndef __OFSPOSCQ_H__
#define __OFSPOSCQ_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O f s P o s c q . h h                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysPthread.hh"

class XrdOss;
class XrdSysError;

class XrdOfsPoscq
{
public:

struct Request
{
long long addT;         // Time committed to the queue
char      LFN[1024];    // Logical File Name (null terminated)
char      User[288];    // User trace identifier
char      Reserved[24]; // Reserved for future
};

static const int ReqOffs  = 64;
static const int ReqSize  = sizeof(Request);

struct recEnt 
{
recEnt        *Next;
int            Offset;
int            Mode;
struct Request reqData;
               recEnt(struct Request &reqref, int mval, recEnt *nval=0)
                     {Next = nval; Offset = 0; Mode = mval; reqData = reqref;}
};

       int     Add(const char *Tident, const char *Lfn);

       int     Commit(const char *Lfn, int Offset);

       int     Del(const char *Lfn, int Offset, int Unlink=0);

       recEnt *Init(int &Ok);

static recEnt *List(XrdSysError *Say, const char *theFN);

inline int     Num() {return pocIQ;}

               XrdOfsPoscq(XrdSysError *erp, XrdOss *oss, const char *fn,
                           int sv=1);
              ~XrdOfsPoscq() {}

private:
void   FailIni(const char *lfn);
int    reqRead(void *Buff, int Offs);
int    reqWrite(void *Buff, int Bsz, int Offs);
int    ReWrite(recEnt *rP);
int    VerOffset(const char *Lfn, int Offset);

struct FileSlot
      {FileSlot *Next;
       int       Offset;
      };

XrdSysMutex  myMutex;
XrdSysError *eDest;
XrdOss      *ossFS;
FileSlot    *SlotList;
FileSlot    *SlotLust;
char        *pocFN;
int          pocSZ;
int          pocFD;
int          pocIQ;
unsigned
short        pocWS;
unsigned
short        pocSV;
};
#endif

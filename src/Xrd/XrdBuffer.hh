#ifndef __XrdBuffer_H__
#define __XrdBuffer_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d B u f f e r . h h                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                            x r d _ B u f f e r                             */
/******************************************************************************/

class XrdBuffer
{
public:

char *   buff;     // -> buffer
int      bsize;    // size of this buffer

         XrdBuffer(char *bp, int sz, int ix)
                      {buff = bp; bsize = sz; bindex = ix; next = 0;}

        ~XrdBuffer() {if (buff) free(buff);}

         friend class XrdBuffManager;
         friend class XrdBuffXL;
private:

int        bindex;
XrdBuffer *next;
static int pagesz;
};
  
/******************************************************************************/
/*                       x r d _ B u f f M a n a g e r                        */
/******************************************************************************/

#define XRD_BUCKETS 12
#define XRD_BUSHIFT 10

// There should be only one instance of this class per buffer pool.
//
class XrdOucTrace;
class XrdSysError;
  
class XrdBuffManager
{
public:

void        Init();

XrdBuffer  *Obtain(int bsz);

int         Recalc(int bsz);

void        Release(XrdBuffer *bp);

int         MaxSize() {return maxsz;}

void        Reshape();

void        Set(int maxmem=-1, int minw=-1);

int         Stats(char *buff, int blen, int do_sync=0);

            XrdBuffManager(XrdSysError *lP, XrdOucTrace *tP, int minrst=20*60);

           ~XrdBuffManager();   // The buffmanager is never deleted

private:

XrdOucTrace *XrdTrace;
XrdSysError *XrdLog;

const int  slots;
const int  shift;
const int  pagsz;
const int  maxsz;

struct {XrdBuffer *bnext;
        int         numbuf;
        int         numreq;
       } bucket[XRD_BUCKETS];          // 1K to 1<<(szshift+slots-1)M buffers

int       totreq;
int       totbuf;
long long totalo;
long long maxalo;
int       minrsw;
int       rsinprog;
int       totadj;

XrdSysCondVar      Reshaper;
static const char *TraceID;
};
#endif

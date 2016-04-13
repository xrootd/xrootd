#ifndef __XrdBuffXL_H__
#define __XrdBuffXL_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d B u f f X L . h h                           */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdBuffer.hh"
#include "XrdSys/XrdSysPthread.hh"

// There should be only one instance of this class.
//
class XrdBuffXL
{
public:

void        Init(int maxMSZ);

XrdBuffer  *Obtain(int bsz);

int         Recalc(int bsz);

void        Release(XrdBuffer *bp);

int         MaxSize() {return maxsz;}

void        Trim();

int         Stats(char *buff, int blen, int do_sync=0);

            XrdBuffXL();

           ~XrdBuffXL() {} // The buffmanager is never deleted

private:

XrdSysMutex       slotXL;

struct BuckVec
      {XrdBuffer *bnext;
       int        numbuf;
       int        numreq;
                  BuckVec() : bnext(0), numbuf(0), numreq(0) {}
                 ~BuckVec() {} // Never gets deleted
       };

       BuckVec   *bucket;  // 4M**(0 ... slots-1) sized buffers
       long long  totalo;

const  int        pagsz;
       int        slots;
       int        maxsz;
       int        totreq;
       int        totbuf;
};
#endif

#ifndef __XRDCKSCALCCRC32_HH__
#define __XRDCKSCALCCRC32_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d C k s C a l c c r c 3 2 . h h                     */
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

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "XrdCks/XrdCksCalc.hh"
#include "XrdSys/XrdSysPlatform.hh"
  
class XrdCksCalccrc32 : public XrdCksCalc
{
public:

char *Final() {char buff[sizeof(long long)];
               long long tLcs = TotLen;
               int i = 0;
               if (tLcs)
                  {while(tLcs) {buff[i++] = tLcs & 0xff ; tLcs >>= 8;}
                   Update(buff, i);
                  }
               TheResult = C32Result ^ CRC32_XOROT;
#ifndef Xrd_Big_Endian
               TheResult = htonl(TheResult);
#endif
               return (char *)&TheResult;
              }

void        Init() {C32Result = CRC32_XINIT; TotLen = 0;}

XrdCksCalc *New() {return (XrdCksCalc *)new XrdCksCalccrc32;}

void        Update(const char *Buff, int BLen);

const char *Type(int &csSz) {csSz = sizeof(TheResult); return "crc32";}

            XrdCksCalccrc32() {Init();}
virtual    ~XrdCksCalccrc32() {}

private:
static const unsigned int CRC32_XINIT = 0;
static const unsigned int CRC32_XOROT = 0xffffffff;
static       unsigned int crctable[256];
             unsigned int C32Result;
             unsigned int TheResult;
             long long    TotLen;
};
#endif

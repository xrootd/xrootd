#ifndef __XRDCKSCALCADLER32_HH__
#define __XRDCKSCALCADLER32_HH__
/******************************************************************************/
/*                                                                            */
/*                  X r d C k s C a l c a d l e r 3 2 . h h                   */
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

#include <sys/types.h>
#include <netinet/in.h>
#include <cinttypes>
#include <zlib.h>

#include "XrdCks/XrdCksCalc.hh"
#include "XrdSys/XrdSysPlatform.hh"

/* The following implementation of adler32 was derived from zlib and is
                   * Copyright (C) 1995-1998 Mark Adler
   Below are the zlib license terms for this implementation.
*/

/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.1.4, March 11th, 2002

  Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

#define DO1(buf)  {unSum1 += *buf++; unSum2 += unSum1;}
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);
#define DO16(buf) DO8(buf); DO8(buf);

class XrdCksCalcadler32 : public XrdCksCalc
{
public:

bool  Combinable() override {return true;}

const char* Combine(const char *Cksum, int DLen) override
            {uint32_t adler2 = getCS(Cksum);
             uint32_t adler1 = (unSum2 << 16) | unSum1;
             uLong newcs = adler32_combine(adler1, adler2, DLen);
             unSum1 = newcs & 0xffff;
             unSum2 = (newcs >> 16) & 0xffff;
             return Final();
            }

const char* Combine(const char* Cksum1, const char* Cksum2, int DLen) override
            {uint32_t adler1 = getCS(Cksum1);
             uint32_t adler2 = getCS(Cksum2);
             AdlerValue = (uint32_t)adler32_combine(adler1, adler2, DLen);
#ifndef Xrd_Big_Endian
             AdlerValue = htonl(AdlerValue);
#endif
             return (const char *)&AdlerValue;
            }

char *Final() override
            {AdlerValue = (unSum2 << 16) | unSum1;
#ifndef Xrd_Big_Endian
             AdlerValue = htonl(AdlerValue);
#endif
             return (char *)&AdlerValue;
            }

void        Init() override {unSum1 = AdlerStart; unSum2 = 0;}

XrdCksCalc *New() override {return (XrdCksCalc *)new XrdCksCalcadler32;}

void        Update(const char *Buff, int BLen) override
                  {int k;
                   unsigned char *buff = (unsigned char *)Buff;
                   while(BLen > 0)
                        {k = (BLen < AdlerNMax ? BLen : AdlerNMax);
                         BLen -= k;
                         while(k >= 16) {DO16(buff); k -= 16;}
                         if (k != 0) do {DO1(buff);} while (--k);
                         unSum1 %= AdlerBase; unSum2 %= AdlerBase;
                        }
                  }

const char *Type(int &csSize) override
                {csSize = sizeof(AdlerValue); return "adler32";}

            XrdCksCalcadler32() {Init();}
virtual    ~XrdCksCalcadler32() {}

private:

uint32_t getCS(const char* csVal)
              {uint32_t aVal;
               memcpy(&aVal, csVal, sizeof(aVal));
#ifndef Xrd_Big_Endian
               aVal = ntohl(aVal);
#endif
               return aVal;
              }

static const uint32_t AdlerBase  = 0xFFF1;
static const uint32_t AdlerStart = 0x0001;
static const int      AdlerNMax  = 5552;

/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

             uint32_t AdlerValue;
             uint32_t unSum1;
             uint32_t unSum2;
};
#endif

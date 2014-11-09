#ifndef __XRDCKSCALCMD5_HH__
#define __XRDCKSCALCMD5_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d C k s C a l c m d 5 . h h                       */
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

#include <stdio.h>

#include "XrdCks/XrdCksCalc.hh"
  
class XrdCksCalcmd5 : public XrdCksCalc
{
public:

char       *Current()
                   {MD5Context saveCTX = myContext;
                    char *md5P = Final();
                    myContext = saveCTX;
                    return (char *)md5P;
                   }

void        Init();

XrdCksCalc *New() {return (XrdCksCalc *)new XrdCksCalcmd5;}

char       *Final();

void        Update(const char *Buff, int BLen)
                  {MD5Update((unsigned char *)Buff,(unsigned)BLen);}

const char *Type(int &csSz) {csSz = sizeof(myDigest); return "md5";}

            XrdCksCalcmd5() {Init();}
           ~XrdCksCalcmd5() {}

private:
  
struct MD5Context
      {unsigned int  buf[4];
union {long long     b64;
       unsigned int  bits[2];
      };
union {long long     i64[8];
       unsigned char in[64];
      };
      };

MD5Context    myContext;
unsigned char myDigest[16];

void byteReverse(unsigned char *buf, unsigned longs);
void MD5Update(unsigned char const *buf, unsigned int len);

#ifndef ASM_MD5
void MD5Transform(unsigned int buf[4], unsigned int const in[16]);
#endif
};
#endif

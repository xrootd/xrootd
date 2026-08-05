/******************************************************************************/
/*                                                                            */
/*                    X r d C k s C a l c c r c 3 2 . c c                     */
/*                                                                            */
/* Copyright (c) 2026 by European Organization of Nuclear Research (CERN)     */
/* Produced by Lukasz Janyst <ljanyst@cern.ch>                                */
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

#include "XrdCks/XrdCksCalccrc32.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include <arpa/inet.h>
#include <cstring>
#include <zlib.h>

/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/

namespace
{
uint32_t getCS(const char* csVal)
              {uint32_t aVal;
               memcpy(&aVal, csVal, sizeof(aVal));
#ifndef Xrd_Big_Endian
               aVal = ntohl(aVal);
#endif
               return aVal;
              }
}

/******************************************************************************/
/*                       X r d C k s C a l c c r c 3 2                        */
/******************************************************************************/

//------------------------------------------------------------------------------
// CRC32 checkum according to the algorithm implemented in zlib
// WARNING: zlib always output the result in the endian byte order that is
//          natural to the machine being used. This version adopts the common
//          standard of representing the checksum in big endian order. To get
//          the explicit zlib version, using the XrdCksCalczcrc32 plugin.
//------------------------------------------------------------------------------

//--------------------------------------------------------------------------
//! Combine checksum into current checksum
//--------------------------------------------------------------------------
const char* XrdCksCalccrc32::Combine(const char* Cksum, int DLen)
{
   uint32_t crc2 = getCS(Cksum);

   pCheckSum = crc32_combine(pCheckSum, crc2, (z_off_t)DLen);

   return (char *)&pCheckSum;
}

//--------------------------------------------------------------------------
//! Combine two checksums and return result
//--------------------------------------------------------------------------
const char* XrdCksCalccrc32::Combine(const char* Cksum1, const char* Cksum2,
                                     int DLen)
{
   uint32_t crc1 = getCS(Cksum1), crc2 = getCS(Cksum2);

   pCheckSumTmp = crc32_combine(crc1, crc2, (z_off_t)DLen);

#ifndef Xrd_Big_Endian
   pCheckSumTmp = ntohl(pCheckSumTmp);
#endif

   return (char *)&pCheckSumTmp;
}

//--------------------------------------------------------------------------
//! Final checksum
//--------------------------------------------------------------------------
char *XrdCksCalccrc32::Final()
{
  pCheckSumTmp = pCheckSum;

#ifndef Xrd_Big_Endian
  pCheckSumTmp = ntohl(pCheckSumTmp);
#endif

  return (char *)&pCheckSumTmp;
}

//--------------------------------------------------------------------------
//! Initialize
//--------------------------------------------------------------------------
void XrdCksCalccrc32::Init()
{
  pCheckSum = crc32( 0L, Z_NULL, 0 );
}

//--------------------------------------------------------------------------
//! Update current checksum
//--------------------------------------------------------------------------
void XrdCksCalccrc32::Update( const char *Buff, int BLen )
{
  pCheckSum = crc32( pCheckSum, (const Bytef*)Buff, BLen );
}

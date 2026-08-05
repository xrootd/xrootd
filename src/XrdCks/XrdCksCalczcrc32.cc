/******************************************************************************/
/*                                                                            */
/*                   X r d C k s C a l c z c r c 3 2 . h h                    */
/*                                                                            */
/* Copyright (c) 2012 by European Organization of Nuclear Research (CERN)     */
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

#include <cstdint>
#include <cstring>
#include <zlib.h>

#include "XrdVersion.hh"
#include "XrdCks/XrdCksCalc.hh"

class XrdSysError;

//------------------------------------------------------------------------------
// CRC32 checkum according to the algorithm implemented in zlib
// WARNING: zlib always output the result in the endian byte order that is
//          natural to the machine being used. Normally, today this is usually
//          little endian and corresponds to RFC1952 for gzip files which is
//          not particularly useful for other file formats. Use crc32 which
//          consistently outputs big endian format regardless of the hardware.
//------------------------------------------------------------------------------

class XrdCksCalczcrc32: public XrdCksCalc
{
  public:

    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    XrdCksCalczcrc32()
    {
      Init();
    }

    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~XrdCksCalczcrc32()
    {
    }

    //--------------------------------------------------------------------------
    //! Combinable trait
    //--------------------------------------------------------------------------
     bool Combinable() override {return true;}

    //--------------------------------------------------------------------------
    //! Combine checksum into current checksum
    //--------------------------------------------------------------------------
    const char* Combine(const char* Cksum, int DLen) override
    {
       uint32_t crc2;
       memcpy(&crc2, Cksum, sizeof(crc2)); // The crc32 as returned by zlib
       pCheckSum = crc32_combine(pCheckSum, crc2, (z_off_t)DLen);
       return (char *)&pCheckSum;
    }

    //--------------------------------------------------------------------------
    //! Combine two checksums and return result
    //--------------------------------------------------------------------------
    const char* Combine(const char* Cksum1, const char* Cksum2, int DLen)
                       override
    {
       uint32_t crc1, crc2;
       memcpy(&crc1, Cksum1, sizeof(crc1)); // The crc32 as returned by zlib
       memcpy(&crc2, Cksum2, sizeof(crc2)); // The crc32 as returned by zlib
       pCheckSumTmp = crc32_combine(crc1, crc2, (z_off_t)DLen);
       return (char *)&pCheckSumTmp;
    }

    //--------------------------------------------------------------------------
    //! Final checksum
    //--------------------------------------------------------------------------
    char *Final() override
    {
      return (char *)&pCheckSum;
    }

    //--------------------------------------------------------------------------
    //! Initialize
    //--------------------------------------------------------------------------
    void Init() override
    {
      pCheckSum = crc32( 0L, Z_NULL, 0 );
    }

    //--------------------------------------------------------------------------
    //! Virtual constructor
    //--------------------------------------------------------------------------
    XrdCksCalc *New() override
    {
      return new XrdCksCalczcrc32();
    }

    //--------------------------------------------------------------------------
    //! Update current checksum
    //--------------------------------------------------------------------------
    void Update( const char *Buff, int BLen ) override
    {
      pCheckSum = crc32( pCheckSum, (const Bytef*)Buff, BLen );
    }

    //--------------------------------------------------------------------------
    //! Checksum algorithm name
    //--------------------------------------------------------------------------
    const char *Type(int &csSz) override
    {
      csSz = 4; return "zcrc32";
    }

  private:
    uint32_t pCheckSum;
    uint32_t pCheckSumTmp;
};

//------------------------------------------------------------------------------
// Plugin callback
//------------------------------------------------------------------------------
extern "C" XrdCksCalc *XrdCksCalcInit(XrdSysError *eDest,
                                      const char  *csName,
                                      const char  *cFN,
                                      const char  *Parms)
{
  return new XrdCksCalczcrc32();
}

XrdVERSIONINFO(XrdCksCalcInit, zcrc32);

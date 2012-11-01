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

#ifndef __XRDCKSCALCZCRC32_HH__
#define __XRDCKSCALCZCRC32_HH__

#include "XrdCks/XrdCksCalc.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdVersion.hh"
#include <stdint.h>
#include <zlib.h>

//------------------------------------------------------------------------------
// CRC32 checkum according to the algorithm implemented in zlib
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
    //! Final checksum
    //--------------------------------------------------------------------------
    char *Final()
    {
      return (char *)&pCheckSum;
    }

    //--------------------------------------------------------------------------
    //! Initialize
    //--------------------------------------------------------------------------
    void Init()
    {
      pCheckSum = crc32( 0L, Z_NULL, 0 );
    }

    //--------------------------------------------------------------------------
    //! Virtual constructor
    //--------------------------------------------------------------------------
    XrdCksCalc *New()
    {
      return new XrdCksCalczcrc32();
    }

    //--------------------------------------------------------------------------
    //! Update current checksum
    //--------------------------------------------------------------------------
    void Update( const char *Buff, int BLen )
    {
      pCheckSum = crc32( pCheckSum, (const Bytef*)Buff, BLen );
    }

    //--------------------------------------------------------------------------
    //! Checksum algorithm name
    //--------------------------------------------------------------------------
    const char *Type(int &csSz)
    {
      csSz = 4; return "zcrc32";
    }

  private:
    uint32_t pCheckSum;
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

#endif // __XRDCKSCALCZCRC32_HH__

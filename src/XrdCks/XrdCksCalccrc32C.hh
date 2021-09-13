#ifndef __XRDCKSCALCCRC32C_HH__
#define __XRDCKSCALCCRC32C_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d C k s C a l c c r c 3 2 C . h h                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <sys/types.h>
#include <netinet/in.h>
#include <cinttypes>

#include "XrdCks/XrdCksCalc.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucCRC.hh"

class XrdCksCalccrc32C : public XrdCksCalc
{
public:
    char *Final();
    
    void Init();
    
    XrdCksCalc *New(); 
    void Update(const char *Buff, int BLen);
    const char *Type(int &csSz);
    XrdCksCalccrc32C(); 
    virtual ~XrdCksCalccrc32C(); 

private:
    static const unsigned int C32C_XINIT = 0;
    unsigned int C32CResult;
    unsigned int TheResult;
};
#endif
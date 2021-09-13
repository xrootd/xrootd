#ifndef __XRDSSISCALE_HH__
#define __XRDSSISCALE_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S c a l e . h h                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <cstring>

#include "XrdSys/XrdSysPthread.hh"

class XrdSsiScale
{
public:

static const uint16_t defSprd =     4; // Initial spread
static const uint16_t maxSprd =  1024; // Maximum spread
static const uint16_t maxPend = 64000; // Maximum pending requests
static const uint16_t minTune =     3; // Minimum remaining channels to tune
static const uint16_t midTune =    64; // Quadratic tuning limit
static const uint16_t maxTune =   128; // Maximum   linear increase
static const uint16_t zipTune =   512; // Channel count when maxTune applies

int   getEnt();

void  retEnt(int xEnt);

bool  rsvEnt(int xEnt);

void  setSpread(short sval);

      XrdSsiScale() : Active(0), reActive(0), begEnt(0), nowEnt(0),
                      curSpread(defSprd), autoTune(false), needTune(false)
                      {memset(pendCnt, 0, sizeof(uint16_t)*maxSprd);}

     ~XrdSsiScale() {}

private:

void        Retune();
bool        Tune(char *buff, int blen);

XrdSysMutex entMutex;
uint32_t    Active;
uint32_t    reActive;
uint16_t    begEnt;
uint16_t    nowEnt;
uint16_t    curSpread;
bool        autoTune;
bool        needTune;
uint16_t    pendCnt[maxSprd];
};
#endif

/******************************************************************************/
/*                                                                            */
/*                        X r d P r o t o c o l . c c                         */
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
#include <string.h>
#include <sys/types.h>

#include "XrdNet/XrdNetSockAddr.hh"
#include "Xrd/XrdProtocol.hh"
  
/******************************************************************************/
/*   X r d P r o t o c o l _ C o n f i g   C o p y   C o n s t r u c t o r    */
/******************************************************************************/
  
XrdProtocol_Config::XrdProtocol_Config(XrdProtocol_Config &rhs)
{
eDest     = rhs.eDest;
NetTCP    = rhs.NetTCP;
BPool     = rhs.BPool;
Sched     = rhs.Sched;
Stats     = rhs.Stats;
theEnv    = rhs.theEnv;
Trace     = rhs.Trace;

ConfigFN  = rhs.ConfigFN ? strdup(rhs.ConfigFN) : 0;
Format    = rhs.Format;
Port      = rhs.Port;
WSize     = rhs.WSize;
AdmPath   = rhs.AdmPath  ? strdup(rhs.AdmPath)  : 0;
AdmMode   = rhs.AdmMode;
myInst    = rhs.myInst   ? strdup(rhs.myInst)   : 0;
myName    = rhs.myName   ? strdup(rhs.myName)   : 0;
         if (!rhs.urAddr) urAddr = 0;
            else {urAddr = (XrdNetSockAddr *)malloc(sizeof(XrdNetSockAddr));
                  memcpy((void *)urAddr, rhs.urAddr, sizeof(XrdNetSockAddr));
                 }
ConnMax   = rhs.ConnMax;
readWait  = rhs.readWait;
idleWait  = rhs.idleWait;
hailWait  = rhs.hailWait;
argc      = rhs.argc;
argv      = rhs.argv;
DebugON   = rhs.DebugON;
WANPort   = rhs.WANPort;
WANWSize  = rhs.WANWSize;
myProg    = rhs.myProg;
}

#ifndef __XRD_LINKINFO_H__
#define __XRD_LINKINFO_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d L i n k I n f o . h h                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>
#include <sys/types.h>
#include <ctime>

#include "XrdSys/XrdSysPthread.hh"

class XrdLinkInfo
{
public:

XrdSysCondVar  *KillcvP;      // Protected by opMutex!
XrdSysSemaphore IOSemaphore;  // Serialization semaphore
time_t          conTime;      // Unix time connected
char           *Etext;        // -> error text, if nil then no error.
XrdSysRecMutex  opMutex;      // Serialization mutex
int             InUse;        // Number of threads using this object
int             doPost;       // Number of threads waiting for serialization
int             FD;           // File descriptor for link use (may be negative)
char            KillCnt;      // Number of times a kill has been attempted

void            Reset()
                     {KillcvP  =  0;
                      conTime  = time(0);
                      if (Etext)    {free(Etext); Etext = 0;}
                      InUse    =  1;
                      doPost   =  0;
                      FD       = -1;
                      KillCnt  =  0;
                     }

                XrdLinkInfo() : IOSemaphore(0, "link i/o"), Etext(0) {Reset();}

               ~XrdLinkInfo() {}
};
#endif

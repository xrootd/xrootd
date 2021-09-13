#ifndef __XRDOSS_STAGE_H__
#define __XRDOSS_STAGE_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O s s S t a g e . h h                         */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctime>
#include <sys/stat.h>
#include "XrdOuc/XrdOucDLlist.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                       X r d O s s S t a g e _ R e q                        */
/******************************************************************************/
  
// Flag values
//
#define XRDOSS_REQ_FAIL 0x00C0
#define XRDOSS_REQ_ENOF 0x0040
#define XRDOSS_REQ_ACTV 0x0001

class XrdOssStage_Req
{
public:

XrdOucDLlist<XrdOssStage_Req> fullList;
XrdOucDLlist<XrdOssStage_Req> pendList;

unsigned long               hash;         // Hash value for the path
const    char              *path;
unsigned long long          size;
int                         flags;
time_t                      sigtod;
int                         prty;

static XrdSysMutex          StageMutex;
static XrdSysSemaphore      ReadyRequest;
static XrdOssStage_Req      StageQ;

       XrdOssStage_Req(unsigned long xhash=0, const char *xpath=0)
                      {fullList.setItem(this); pendList.setItem(this);
                       hash  = xhash; path = (xpath ? strdup(xpath) : 0);
                       flags=0; sigtod=0; size= 2ULL<<31LL; prty=0;
                      }

       XrdOssStage_Req(XrdOssStage_Req *that)
                      {fullList.setItem(that); pendList.setItem(that);
                       hash  = 0; path = 0; flags=0; sigtod=0; size= 0; prty=0;
                      }

      ~XrdOssStage_Req() {if (path) free((void *)path);
                          fullList.Remove();
                          pendList.Remove();
                         }
};
#endif

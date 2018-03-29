#ifndef __XRDCMSRTABLE_HH_
#define __XRDCMSRTABLE_HH_
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s R T a b l e . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdSys/XrdSysPthread.hh"
  
class XrdCmsRTable
{
public:

short         Add(XrdCmsNode *nP);

void          Del(XrdCmsNode *nP);

XrdCmsNode   *Find(short Num, int Inst);

void          Send(const char *What, const char *data, int dlen);

void          Lock() {myMutex.Lock();}

void          UnLock() {myMutex.UnLock();}

              XrdCmsRTable() {memset(Rtable, 0, sizeof(Rtable)); Hwm = -1;}

             ~XrdCmsRTable() {}

private:

XrdSysMutex   myMutex;
XrdCmsNode   *Rtable[maxRD];
int           Hwm;
};

namespace XrdCms
{
extern    XrdCmsRTable RTable;
}
#endif

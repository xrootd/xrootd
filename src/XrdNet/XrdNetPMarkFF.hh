#ifndef __XRDNETPMARKFF__
#define __XRDNETPMARKFF__
/******************************************************************************/
/*                                                                            */
/*                      X r d N e t P M a r k F F . h h                       */
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

#include "XrdNet/XrdNetPMark.hh"

class XrdNetAddrInfo;

class XrdNetPMarkFF : public XrdNetPMark::Handle
{
public:

void         addHandle(XrdNetPMark::Handle *fh) {xtraFH = fh;}

bool         Start(XrdNetAddrInfo &addr);

             XrdNetPMarkFF(XrdNetPMark::Handle &h, const char *tid)
                          : XrdNetPMark::Handle(h), tident(tid), xtraFH(0)  {}

virtual     ~XrdNetPMarkFF();

private:

bool        Emit(const char *state, const char *cT, const char *eT);
const char *getUTC(char *utcBuff, int utcBLen);

char       *ffHdr     = 0;
char       *ffTailC   = 0;
int         ffTailCsz = 0;
int         ffTailSsz = 0; // Set to zero if any part failed!
char       *ffTailS   = 0;
const char *tident;

XrdNetPMark::Handle *xtraFH;
};
#endif

#ifndef _XRDOSSACSTAGE_H
#define _XRDOSSACSTAGE_H
/******************************************************************************/
/*                                                                            */
/*                     X r d O s s A r c S t a g e . h h                      */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>

#include "Xrd/XrdJob.hh"

class XrdOucProg;

class XrdOssArcStage : public XrdJob
{
public:

virtual  void DoIt() override;

static   int  Stage(const char* path);

         XrdOssArcStage(const char* aPath) : XrdJob("Arc Staging"),
                        arcvPath(aPath) {}

virtual ~XrdOssArcStage() {}

private:

enum MssRC {isBad = -1, isFalse = 0, isTrue = 1};

static MssRC isOnline(const char* path);
       void  Reset(const char* path);
       void  StageError(int rc, const char* what, const char* path);

const char* arcvPath; // Valid if present in Active set.
};
#endif

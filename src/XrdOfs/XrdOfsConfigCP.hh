#ifndef __XRDOFSCONFIGCP_HH__
#define __XRDOFSCONFIGCP_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d O f s C o n f i g C P . h h                      */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdOucStream;

class XrdOfsConfigCP
{
public:

static char     *Path;
static long long MaxSZ;
static int       MaxVZ;
static bool      cprErrNA;
static bool      Enabled;
static bool      isProxy;
static bool      EnForce;

static bool Init();

static bool Parse(XrdOucStream &Config);

private:
struct Stats
      {int  numFiles;
       int  numRecov;
       int  numError;
       int  numSkipd;
       int  numUnres;
            Stats() :numFiles(0), numRecov(0), numError(0),
                     numSkipd(0), numUnres(0) {}
           ~Stats() {}
      };

static
void   Recover(const char *ckpPath, struct Stats &stats);

            XrdOfsConfigCP() {}
           ~XrdOfsConfigCP() {}
};
#endif

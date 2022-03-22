#ifndef __XRDOFSTPCCONFIG_HH__
#define __XRDOFSTPCCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O f s T p c C o n f i g . h h                     */
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

class XrdXrootdTpcMon;

struct XrdOfsTPCConfig
{
XrdXrootdTpcMon* tpcMon;

char  *XfrProg;
char  *cksType;
char  *cPath;
char  *rPath;
int    maxTTL;
int    dflTTL;
int    tcpSTRM;
int    tcpSMax;
int    xfrMax;
int    errMon;
bool   LogOK;
bool   doEcho;
bool   autoRM;
bool   noids;
bool   fCreds;

       XrdOfsTPCConfig() : tpcMon(0), XfrProg(0), cksType(0), cPath(0), rPath(0),
                           maxTTL(15), dflTTL(7),  tcpSTRM(0),   tcpSMax(15),
                           xfrMax(9),  errMon(-3), LogOK(false), doEcho(false),
                           autoRM(false), noids(true), fCreds(false)
                           {}

      ~XrdOfsTPCConfig() {} // Never deleted
};
#endif

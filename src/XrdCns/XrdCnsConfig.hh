#ifndef __XRDCNSCONFIG_H__
#define __XRDCNSCONFIG_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C n s C o n f i g . h h                        */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdCnsLogServer;
class XrdCnsXref;
class XrdOucTList;
class XrdOucName2Name;

class XrdCnsConfig
{
public:

char             *aPath;       // Admin       path
char             *bPath;       // Backup      path
char             *cPath;       // Config file path
char             *ePath;       // Event  file path (where logfiles  go)
XrdOucTList      *Dest;        // Redir  list      (where namespace goes)
XrdOucTList      *bDest;       // Bkup   entry     (where backup    goes)
XrdOucTList      *Exports;     // Local  exports
char             *LCLRoot;
XrdOucName2Name  *N2N;
XrdCnsLogServer  *XrdCnsLog;
XrdCnsXref       *Space;
char             *logfn;       // Logmsg path
int               bindArg;
int               Port;        // Xroot server port number for  Dest hosts
int               mInt;        // Check interval for Inventory file
int               cInt;        // Close interval for logfiles
int               qLim;        // Close count    for logfiles
int               Opts;

static const int  optRecr = 0x0001;
static const int  optNoCns= 0x0002;

int               Configure(int argc, char **argv, char *argt=0);

int               Configure();

int               LocalPath(const char *oldp, char *newp, int newpsz);

int               LogicPath(const char *oldp, char *newp, int newpsz);

int               MountPath(const char *oldp, char *newp, int newpsz);

                  XrdCnsConfig() : aPath(0), bPath(0), cPath(0), ePath(0),
                                   Dest(0),  bDest(0), Exports(0),
                                   LCLRoot(0), N2N(0), XrdCnsLog(0), Space(0),
                                   logfn(0), bindArg(0), Port(1095),
                                   mInt(1800), cInt(1200), qLim(512), Opts(0)
                                 {}
                 ~XrdCnsConfig() {}

private:
int ConfigN2N();
int NAPath(const char *What, const char *Path);
};
#endif

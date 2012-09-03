#ifndef __XRDCNSSSICFG_H__
#define __XRDCNSSSICFG_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C n s S s i C f g . h h                        */
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
class XrdOucTList;
class XrdOucName2Name;
class XrdSysLogger;

class XrdCnsSsiCfg
{
public:

char             *bPath;      // Backup path
char             *logFN;      // Logmsg path
XrdOucTList      *dirList;    // Backup directories (refreshed)
const char       *Func;
char              Xeq;        // What we will be doing
char              Lopt;
char              Verbose;

static const char Lmode = 0x01;
static const char Lsize = 0x02;
static const char Lfmts = 0x80;
static const char Lhost = 0x04;
static const char Lname = 0x08;
static const char Lmount= 0x10;
static const char Lfull = 0x17;

int               Configure(int argc, char **argv);

int               Configure(int argc, char **argv, const char *Opts);

                  XrdCnsSsiCfg() : bPath(0), dirList(0), Func("?"),
                                   Xeq(0), Lopt(0),
                                   Verbose(0)
                                 {}
                 ~XrdCnsSsiCfg() {}
private:
void Usage(const char *T1, const char *T2=0);
};
#endif

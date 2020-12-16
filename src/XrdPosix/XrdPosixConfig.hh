#ifndef __XRDPOSIXCONFIG_H__
#define __XRDPOSIXCONFIG_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x C o n f i g . h h                      */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/

#include <unistd.h>
#include <sys/types.h>

class XrdOucEnv;
class XrdOucPsx;
class XrdScheduler;
class XrdPosixInfo;
class XrdSecsssCon;
class XrdSysLogger;

struct stat;

class XrdPosixConfig
{
public:

static
XrdSecsssCon  *conTracker(bool debug=false);

static void    EnvInfo(XrdOucEnv &theEnv);

static void    initStat(struct stat *buf);

static bool    SetConfig(XrdOucPsx &parms);

static bool    OpenFC(const char *path, int oflag, mode_t mode,
                      XrdPosixInfo &Info);

static void    SetEnv(const char *kword, int kval);

static void    setOids(bool isok);

static int     Stats(const char *theID, char *buff, int blen);

               XrdPosixConfig() {}
              ~XrdPosixConfig() {}

private:
static bool initCCM(XrdOucPsx &parms);
static void initEnv(char *eData);
static void initEnv(XrdOucEnv &, const char *, long long &);
static void initXdev(dev_t &st_dev, dev_t &st_rdev);
static void SetDebug(int val);
static void SetIPV4(bool userv4);
};
#endif

#ifndef __XROOTD_PREPARE__H
#define __XROOTD_PREPARE__H
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d P r e p a r e . h h                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <dirent.h>
#include <sys/types.h>
  
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTList.hh"

/******************************************************************************/
/*                        X r d O l b P r e p A r g s                         */
/******************************************************************************/

class XrdXrootdPrepArgs
{
public:
friend class XrdXrootdPrepare;

char        *reqid;
char        *user;
char        *notify;
int          prty;
char         mode[4];
XrdOucTList *paths;

             XrdXrootdPrepArgs(int sfree=1,  int pfree=1)
                {reqid = user = notify = 0; paths = 0; *mode = '\0';
                 dirP = 0; prty = reqlen = usrlen = 0;
                 freestore = sfree; freepaths = pfree;
                }
            ~XrdXrootdPrepArgs()
                {XrdOucTList *tp;
                 if (freestore)
                    {if (reqid)  free(reqid);
                     if (notify) free(notify);
                    }
                 if (freepaths) while((tp=paths)) {paths=paths->next; delete tp;}
                 if (dirP) closedir(dirP);
                }
private:
DIR *dirP;
int reqlen;
int usrlen;
int freestore;
int freepaths;
};
  
/******************************************************************************/
/*                   C l a s s   X r d O l b P r e p a r e                    */
/******************************************************************************/
  
class XrdXrootdPrepare : public XrdJob
{
public:

static int        Close(int fd) {return close(fd);}

       void       DoIt() {Scrub();
                          SchedP->Schedule((XrdJob *)this, scrubtime+time(0));
                         }

static int        List(XrdXrootdPrepArgs &pargs, char *resp, int resplen);

static void       Log(XrdXrootdPrepArgs &pargs);

static void       Logdel(char *reqid);

static int        Open(const char *reqid, int &fsz);

static void       Scrub();

static int        setParms(int stime, int skeep);

static int        setParms(char *ldir);

           XrdXrootdPrepare(XrdSysError *lp, XrdScheduler *sp);
          ~XrdXrootdPrepare() {}   // Never gets deleted

private:

static const char    *TraceID;
static XrdScheduler  *SchedP;    // System scheduler
static XrdSysError   *eDest;     // Error message handler

static int            scrubtime;
static int            scrubkeep;
static char          *LogDir;
static int            LogDirLen;
};
#endif

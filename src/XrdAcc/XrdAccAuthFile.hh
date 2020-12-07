#ifndef __ACC_AUTHFILE__
#define __ACC_AUTHFILE__
/******************************************************************************/
/*                                                                            */
/*                     X r d A c c A u t h F i l e . h h                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <limits.h>
#include <netdb.h>
#include <sys/param.h>
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdAcc/XrdAccAuthDB.hh"

// This class is provided for obtaining capability information from a file.
//
class XrdAccAuthFile : public XrdAccAuthDB
{
public:

int      Open(XrdSysError &eroute, const char *path=0);

char     getRec(char **recname);

char     getID(char **id);

int      getPP(char **path, char **priv, bool &istmplt);

int      Close();

int      Changed(const char *dbpath);

         XrdAccAuthFile(XrdSysError *erp);
        ~XrdAccAuthFile();

private:

int  Bail(int retc, const char *txt1, const char *txt2=0);
char *Copy(char *dp, char *sp, int dplen);

enum DBflags {Noflags=0, inRec=1, isOpen=2, dbError=4}; // Values combined

XrdSysError      *Eroute;
DBflags           flags;
XrdOucStream      DBfile;
char             *authfn;
char              rectype;
time_t            modtime;
XrdSysMutex       DBcontext;

char recname_buff[MAXHOSTNAMELEN+1];   // Max record name by default
char path_buff[MAXPATHLEN+2];          // Max path   name
};
#endif

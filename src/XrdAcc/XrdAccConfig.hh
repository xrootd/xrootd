#ifndef _ACC_CONFIG_H
#define _ACC_CONFIG_H
/******************************************************************************/
/*                                                                            */
/*                       X r d A c c C o n f i g . h h                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include <sys/types.h>

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdAcc/XrdAccAccess.hh"
#include "XrdAcc/XrdAccAuthDB.hh"
#include "XrdAcc/XrdAccCapability.hh"
#include "XrdAcc/XrdAccGroups.hh"

/******************************************************************************/
/*                           X r d A c c G l i s t                            */
/******************************************************************************/
  
struct XrdAccGlist 
{
       struct XrdAccGlist *next;     /* Null if this is the last one */
       char               *name;     /* -> null terminated name */

       XrdAccGlist(const char *Name, struct XrdAccGlist *Next=0)
                  {name = strdup(Name); next = Next;}
      ~XrdAccGlist()
                  {if (name) free(name);}
};

/******************************************************************************/
/*                          X r d A c c C o n f i g                           */
/******************************************************************************/
  
class XrdAccConfig
{
public:

// Configure() is called during initialization.
//
int           Configure(XrdSysError &Eroute, const char *cfn);

// ConfigDB() simply refreshes the in-core authorization database. When the 
// Warm is true, a check is made whether the database actually changed and the
// refresh is skipped if it has not changed.
//
int           ConfigDB(int Warm, XrdSysError &Eroute);

XrdAccAccess *Authorization;
XrdAccGroups  GroupMaster;

int           AuthRT;

              XrdAccConfig();
             ~XrdAccConfig() {}    // Configuration is never destroyed!

private:

struct XrdAccGlist *addGlist(gid_t Gid, const char *Gname, 
                             struct XrdAccGlist *Gnext);
int                 ConfigDBrec(XrdSysError &Eroute,
                                struct XrdAccAccess_Tables &tabs);
void                ConfigDefaults(void);
int                 ConfigFile(XrdSysError &Eroute, const char *cfn);
int                 ConfigXeq(char *, XrdOucStream &, XrdSysError &);
void                idChk(XrdSysError &Eroute, XrdAccAccess_ID *idList,
                          XrdAccAccess_Tables &tabs);
int                 idDef(XrdSysError &Eroute, XrdAccAccess_Tables &tabs,
                          const char *idName);
void                subSpace(char *id);
int                 PrivsConvert(char *privs, XrdAccPrivCaps &ctab);

int                 xaud(XrdOucStream &Config, XrdSysError &Eroute);
int                 xart(XrdOucStream &Config, XrdSysError &Eroute);
int                 xdbp(XrdOucStream &Config, XrdSysError &Eroute);
int                 xglt(XrdOucStream &Config, XrdSysError &Eroute);
int                 xgrt(XrdOucStream &Config, XrdSysError &Eroute);
int                 xnis(XrdOucStream &Cofig, XrdSysError &Eroute);
int                 xspc(XrdOucStream &Cofig, XrdSysError &Eroute);

XrdAccAuthDB        *Database;
char                *dbpath;

XrdSysMutex          Config_Context;
XrdSysThread         Config_Refresh;

int                  options;
int                  rulenum;
char                 spChar;
};
#endif

#ifndef __ACC_ACCESS__
#define __ACC_ACCESS__
/******************************************************************************/
/*                                                                            */
/*                       X r d A c c A c c e s s . h h                        */
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

#include "XrdAcc/XrdAccAudit.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdAcc/XrdAccCapability.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysXSLock.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                     S e t T a b s   P a r a m e t e r                      */
/******************************************************************************/

struct XrdAccEntityInfo;

struct XrdAccAccess_ID
       {char             *name;
        char             *grp;
        char             *host;
        char             *org;
        char             *role;
        char             *user;
        XrdAccCapability *caps;
        XrdAccAccess_ID  *next;
        int               rule;
        short             hlen;
        short             glen;

        bool             Applies(const XrdAccEntityInfo &Entity);

        XrdAccAccess_ID *Export()
                         {XrdAccAccess_ID *xID;
                          xID = new XrdAccAccess_ID;
                          *xID = *this;
                          name = grp = host = org = role = user = 0;
                          caps = 0;
                          return xID;
                         }

              XrdAccAccess_ID(const char *Name=0)
                             : name(Name ? strdup(Name) : 0),
                               grp(0), host(0), org(0), role(0), user(0),
                               caps(0), next(0), rule(0), hlen(0), glen(0) {}
             ~XrdAccAccess_ID() {if (name) free(name);
                                 if (grp)  free(grp);
                                 if (host) free(host);
                                 if (org)  free(org);
                                 if (role) free(role);
                                 if (user) free(user);
                                 if (caps) delete caps;
                                }
       };
  
struct XrdAccAccess_Tables
       {XrdOucHash<XrdAccCapability> *G_Hash;  // Groups
        XrdOucHash<XrdAccCapability> *H_Hash;  // Hosts
        XrdOucHash<XrdAccCapability> *N_Hash;  // Netgroups
        XrdOucHash<XrdAccCapability> *O_Hash;  // Organizations
        XrdOucHash<XrdAccCapability> *R_Hash;  // Roles
        XrdOucHash<XrdAccAccess_ID>  *S_Hash;  // Sets
        XrdOucHash<XrdAccCapability> *T_Hash;  // Templates
        XrdOucHash<XrdAccCapability> *U_Hash;  // Users
                  XrdAccCapName     *D_List;  // Domains
                  XrdAccCapName     *E_List;  // Domains (end of list)
                  XrdAccCapability  *X_List;  // Fungable capbailities
                  XrdAccCapability  *Z_List;  // Default  capbailities
                  XrdAccAccess_ID   *SXList;  // 's' exclusive list
                  XrdAccAccess_ID   *SYList;  // 's' inclusive list

        XrdAccAccess_Tables() {G_Hash = 0; H_Hash = 0; N_Hash = 0;
                               O_Hash = 0; R_Hash = 0;
                               S_Hash = 0; T_Hash = 0; U_Hash = 0;
                               D_List = 0; E_List = 0;
                               X_List = 0; Z_List = 0;
                               SXList = 0; SYList = 0;
                              }
       ~XrdAccAccess_Tables() {if (G_Hash) delete G_Hash;
                               if (H_Hash) delete H_Hash;
                               if (N_Hash) delete N_Hash;
                               if (O_Hash) delete O_Hash;
                               if (R_Hash) delete R_Hash;
                               if (S_Hash) delete S_Hash; //Deletes SX & SYList
                               if (T_Hash) delete T_Hash;
                               if (U_Hash) delete U_Hash;
                               if (X_List) delete X_List;
                               if (Z_List) delete Z_List;
                              }
       };

/******************************************************************************/
/*                          X r d A c c A c c e s s                           */
/******************************************************************************/

class xrdOucError;
  
class XrdAccAccess : public XrdAccAuthorize
{
public:

friend class XrdAccConfig;

      XrdAccPrivs Access(const XrdSecEntity    *Entity,
                         const char            *path,
                         const Access_Operation oper,
                               XrdOucEnv       *Env=0);

      int         Audit(const int              accok,
                        const XrdSecEntity    *Entity,
                        const char            *path,
                        const Access_Operation oper,
                               XrdOucEnv      *Env=0);

static
const char       *Resolve(const XrdSecEntity *Entity);

// SwapTabs() is used by the configuration object to establish new access
// control tables. It may be called whenever the tables change.
//
void              SwapTabs(struct XrdAccAccess_Tables &newtab);

      int Test(const XrdAccPrivs priv, const Access_Operation oper);

      XrdAccAccess(XrdSysError *erp);

     ~XrdAccAccess() {} // The access object is never deleted

private:

XrdAccPrivs Access(      XrdAccPrivCaps  &caps,
                   const XrdSecEntity    *Entity,
                   const char            *path,
                   const Access_Operation oper);

struct XrdAccAccess_Tables Atab;
bool   hostRefX; // True if we need to resolve hostname for exclusive rules
bool   hostRefY; // True if we need to resolve hostname for any other rules

XrdSysXSLock Access_Context;

XrdAccAudit *Auditor;
};
#endif

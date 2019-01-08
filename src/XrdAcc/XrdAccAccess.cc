/******************************************************************************/
/*                                                                            */
/*                       X r d A c c A c c e s s . c c                        */
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

#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <sys/param.h>

#include "XrdVersion.hh"

#include "XrdAcc/XrdAccAccess.hh"
#include "XrdAcc/XrdAccCapability.hh"
#include "XrdAcc/XrdAccConfig.hh"
#include "XrdAcc/XrdAccGroups.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysPlugin.hh"
  
/******************************************************************************/
/*                   E x t e r n a l   R e f e r e n c e s                    */
/******************************************************************************/
  
extern unsigned long XrdOucHashVal2(const char *KeyVal, int KeyLen);

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/
  
extern XrdAccConfig XrdAccConfiguration;

/******************************************************************************/
/*       Autorization Object Creation via XrdAccDefaultAuthorizeObject        */
/******************************************************************************/
  
XrdAccAuthorize *XrdAccDefaultAuthorizeObject(XrdSysLogger *lp,
                                              const char *cfn,
                                              const char *parm,
                                              XrdVersionInfo &urVer)
{
   static XrdVERSIONINFODEF(myVer, XrdAcc, XrdVNUMBER, XrdVERSION);
   static XrdSysError Eroute(lp, "acc_");

// Verify version compatability
//
   if (urVer.vNum != myVer.vNum && !XrdSysPlugin::VerCmp(urVer,myVer))
      return 0;

// Configure the authorization system
//
   if (XrdAccConfiguration.Configure(Eroute, cfn)) return (XrdAccAuthorize *)0;

// All is well, return the actual pointer to the object
//
   return (XrdAccAuthorize *)XrdAccConfiguration.Authorization;
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdAccAccess::XrdAccAccess(XrdSysError *erp)
{
// Get the audit option that we should use
//
   Auditor = XrdAccAuditObject(erp);
}

/******************************************************************************/
/*                                A c c e s s                                 */
/******************************************************************************/
  
XrdAccPrivs XrdAccAccess::Access(const XrdSecEntity    *Entity,
                                 const char            *path,
                                 const Access_Operation oper,
                                       XrdOucEnv       *Env)
{
   const char *xP;
   char *gname, xBuff[64];
   XrdAccGroupList *glp;
   XrdAccPrivCaps caps;
   XrdAccCapability *cp;
   const int plen  = strlen(path);
   const long phash = XrdOucHashVal2(path, plen);
   const char *id   = (Entity->name ? (const char *)Entity->name : "*");
   const char *host = 0;
   int n, isuser = (*id && (*id != '*' || id[1]));

// Get a shared context for these potentially long running routines
//
   Access_Context.Lock(xs_Shared);

// Run through the exclusive list first as only one rule will apply
//
   XrdAccAccess_ID *xlP = Atab.SXList;
   while (xlP)
         {if (xlP->Applies(Entity))
             {xlP->caps->Privs(caps, path, plen, phash);
              Access_Context.UnLock(xs_Shared);
              return Access(caps, Entity, path, oper);
             }
          xlP = xlP->next;
         }

// Check if we really need to resolve the host name
//
   if (Atab.D_List || Atab.H_Hash || Atab.N_Hash) host = Resolve(Entity);

// Establish default privileges
//
   if (Atab.Z_List) Atab.Z_List->Privs(caps, path, plen, phash);

// Next add in the host domain privileges
//
   if (Atab.D_List && host && (cp = Atab.D_List->Find(host)))
      cp->Privs(caps, path, plen, phash);

// Next add in the host-specific privileges
//
   if (Atab.H_Hash && host && (cp = Atab.H_Hash->Find(host)))
      cp->Privs(caps, path, plen, phash);

// Check for user fungible privileges
//
   if (isuser && Atab.X_List) Atab.X_List->Privs(caps, path, plen, phash, id);

// Add in specific user privileges
//
   if (isuser && Atab.U_Hash && (cp = Atab.U_Hash->Find(id)))
      cp->Privs(caps, path, plen, phash);

// Next add in the group privileges. The group list either comes from the
// credentials, in which case we need not have a username, or from the
// standard unix-username group mapping.
//
   if (Atab.G_Hash)
      {if (Entity->grps)
          {xP = Entity->grps;
           while((n = XrdOucUtils::Token(&xP, ' ', xBuff, sizeof(xBuff))))
                {if (n < (int)sizeof(xBuff) && (cp = Atab.G_Hash->Find(xBuff)))
                    cp->Privs(caps, path, plen, phash);
                }
          } else if (isuser && (glp=XrdAccConfiguration.GroupMaster.Groups(id)))
                    {while((gname = (char *)glp->Next()))
                          if ((cp = Atab.G_Hash->Find((const char *)gname)))
                             cp->Privs(caps, path, plen, phash);
                     delete glp;
                    }
      }

// Now add in the netgroup privileges
//
   if (Atab.N_Hash && id && host && 
       (glp = XrdAccConfiguration.GroupMaster.NetGroups(id, host)))
      {while((gname = (char *)glp->Next()))
            if ((cp = Atab.N_Hash->Find((const char *)gname)))
               cp->Privs(caps, path, plen, phash);
       delete glp;
      }

// Next add in the org-specific privileges
//
   if (Atab.O_Hash && Entity->vorg)
      {xP = Entity->vorg;
       while((n = XrdOucUtils::Token(&xP, ' ', xBuff, sizeof(xBuff))))
            {if (n < (int)sizeof(xBuff) && (cp = Atab.O_Hash->Find(xBuff)))
                cp->Privs(caps, path, plen, phash);
            }
      }

// Next add in the role-specific privileges
//
   if (Atab.R_Hash && Entity->role)
      {xP = Entity->role;
       while((n = XrdOucUtils::Token(&xP, ' ', xBuff, sizeof(xBuff))))
            {if (n < (int)sizeof(xBuff) && (cp = Atab.R_Hash->Find(xBuff)))
                cp->Privs(caps, path, plen, phash);
            }
      }

// Finally run through the inclusive list and apply arr relevant rules
//
   XrdAccAccess_ID *ylP = Atab.SYList;
   while (ylP)
         {if (ylP->Applies(Entity)) ylP->caps->Privs(caps, path, plen, phash);
          ylP = ylP->next;
         }

// We are now done with looking at changeable data
//
   Access_Context.UnLock(xs_Shared);

// Return the privileges as needed
//
   return Access(caps, Entity, path, oper);
}

/******************************************************************************/

XrdAccPrivs XrdAccAccess::Access(      XrdAccPrivCaps  &caps,
                                 const XrdSecEntity    *Entity,
                                 const char            *path,
                                 const Access_Operation oper
                                )
{
   XrdAccPrivs myprivs;
   XrdAccAudit_Options audits = (XrdAccAudit_Options)Auditor->Auditing();
   int accok;

// Compute composite privileges and see if privs need to be returned
//
   myprivs = (XrdAccPrivs)(caps.pprivs & ~caps.nprivs);
   if (!oper) return (XrdAccPrivs)myprivs;

// Check if auditing is enabled or whether we can do a fastaroo test
//
   if (!audits) return (XrdAccPrivs)Test(myprivs, oper);
   if ((accok = Test(myprivs, oper)) && !(audits & audit_grant))
      return (XrdAccPrivs)accok;

// Call the auditing routine and exit
//
   return (XrdAccPrivs)Audit(accok, Entity, path, oper);
}
  
/******************************************************************************/
/*                                 A u d i t                                  */
/******************************************************************************/
  
int XrdAccAccess::Audit(const int              accok,
                        const XrdSecEntity    *Entity,
                        const char            *path,
                        const Access_Operation oper,
                              XrdOucEnv       *Env)
{
// Warning! This table must be in 1-to-1 correspondence with Access_Operation
//
   static const char *Opername[] = {"any",             // 0
                                    "chmod",           // 1
                                    "chown",           // 2
                                    "create",          // 3
                                    "delete",          // 4
                                    "insert",          // 5
                                    "lock",            // 6
                                    "mkdir",           // 7
                                    "read",            // 8
                                    "readdir",         // 9
                                    "rename",          // 10
                                    "stat",            // 10
                                    "update"           // 12
                             };
   const char *opname = (oper > AOP_LastOp ? "???" : Opername[oper]);
   const char *id   = (Entity->name ? (const char *)Entity->name : "*");
   const char *host = (Entity->host ? (const char *)Entity->host : "?");
   char atype[XrdSecPROTOIDSIZE+1];

// Get the protocol type in a printable format
//
   strncpy(atype, Entity->prot, XrdSecPROTOIDSIZE);
   atype[XrdSecPROTOIDSIZE] = '\0';

// Route the message appropriately
//
    if (accok) Auditor->Grant(opname, Entity->tident, atype, id, host, path);
       else    Auditor->Deny( opname, Entity->tident, atype, id, host, path);

// All done, finally
//
   return accok;
}

/******************************************************************************/
/*                               R e s o l v e                                */
/******************************************************************************/

const char *XrdAccAccess::Resolve(const XrdSecEntity *Entity)
{
// Make a quick test for IPv6 (as that's the future) and a minimal one for ipV4
// to see if we have to do a DNS lookup.
//
   if (Entity->host == 0 || *(Entity->host) == '[' || isdigit(*(Entity->host)))
      return  Entity->addrInfo->Name("?");
   return Entity->host;
}
  
/******************************************************************************/
/*                              S w a p T a b s                               */
/******************************************************************************/

#define XrdAccSWAP(x) oldtab.x = Atab.x;   Atab.x  =  newtab.x; \
                      newtab.x = oldtab.x; oldtab.x = 0;

void XrdAccAccess::SwapTabs(struct XrdAccAccess_Tables &newtab)
{
     struct XrdAccAccess_Tables oldtab;

// Get an exclusive context to change the table pointers
//
   Access_Context.Lock(xs_Exclusive);

// Save the old pointer while replacing it with the new pointer
//
   XrdAccSWAP(D_List);
   XrdAccSWAP(E_List);
   XrdAccSWAP(G_Hash);
   XrdAccSWAP(H_Hash);
   XrdAccSWAP(N_Hash);
   XrdAccSWAP(O_Hash);
   XrdAccSWAP(R_Hash);
   XrdAccSWAP(S_Hash);
   XrdAccSWAP(T_Hash);
   XrdAccSWAP(U_Hash);
   XrdAccSWAP(X_List);
   XrdAccSWAP(Z_List);
   XrdAccSWAP(SXList);
   XrdAccSWAP(SYList);

// When we set new access tables, we should purge the group cache
//
   XrdAccConfiguration.GroupMaster.PurgeCache();

// We can now let loose new table searchers
//
   Access_Context.UnLock(xs_Exclusive);
}

/******************************************************************************/
/*                                  T e s t                                   */
/******************************************************************************/

int XrdAccAccess::Test(const XrdAccPrivs priv,const Access_Operation oper)
{

// Warning! This table must be in 1-to-1 correspondence with Access_Operation
//
   static XrdAccPrivs need[] = {XrdAccPriv_None,                 // 0
                                XrdAccPriv_Chmod,                // 1
                                XrdAccPriv_Chown,                // 2
                                XrdAccPriv_Create,               // 3
                                XrdAccPriv_Delete,               // 4
                                XrdAccPriv_Insert,               // 5
                                XrdAccPriv_Lock,                 // 6
                                XrdAccPriv_Mkdir,                // 7
                                XrdAccPriv_Read,                 // 8
                                XrdAccPriv_Readdir,              // 9
                                XrdAccPriv_Rename,               // 10
                                XrdAccPriv_Lookup,               // 11
                                XrdAccPriv_Update                // 12
                               };
   if (oper < 0 || oper > AOP_LastOp) return 0;
   return (int)(need[oper] & priv) == need[oper];
}

/******************************************************************************/
/*              X r d A c c A c c e s s _ I D : : A p p l i e s               */
/******************************************************************************/
  
bool XrdAccAccess_ID::Applies(const XrdSecEntity *Entity)
{
   const char *hName, *gList, *gEnd;
   int eLen;

// Check single value items in the most probable use order
//
   if (org  && (!Entity->vorg || strcmp(org,  Entity->vorg))) return false;
   if (role && (!Entity->role || strcmp(role, Entity->role))) return false;
   if (user && (!Entity->name || strcmp(user, Entity->name))) return false;

// The check is more complicated as the host field may be an address. We make
// a quick test for IPv6 (as that's the future) and take the long road for ipV4.
//
   if (host)
      {hName = XrdAccAccess::Resolve(Entity);
       if (*host == '.')
          {eLen = strlen(hName);
           if (eLen <= hlen) return false;
           hName = hName + eLen - hlen;
          }
       if (strcmp(host, hName)) return false;
      }

// Groups are most problematic as there may be many of them. So it's last.
//
   if (!grp) return true;
   if (!Entity->grps) return false;
   eLen = strlen(Entity->grps);
   if (eLen < glen) return false;

// Search through the group list
//
   gList = Entity->grps;
   while(true)
        {if (!strncmp(grp, Entity->grps, glen))
            {gEnd = Entity->grps + glen;
             if (*gEnd == ' ' || *gEnd == 0) return true;
            }
         if(!(gList = index(gList, ' '))) break;
         do {gList++;} while(*gList == ' ');
        }

// This entry is not applicable
//
   return false;
}

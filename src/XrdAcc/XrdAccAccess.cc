/******************************************************************************/
/*                                                                            */
/*                       X r d A c c A c c e s s . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdAccAccessCVSID = "$Id$";

#include <stdio.h>
#include <time.h>
#include <sys/param.h>

#include "XrdAcc/XrdAccAccess.hh"
#include "XrdAcc/XrdAccCapability.hh"
#include "XrdAcc/XrdAccConfig.hh"
#include "XrdAcc/XrdAccGroups.hh"
  
/******************************************************************************/
/*                   E x t e r n a l   R e f e r e n c e s                    */
/******************************************************************************/
  
extern unsigned long XrdOucHashVal2(const char *KeyVal, int KeyLen);

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/
  
extern XrdAccConfig XrdAccConfiguration;

/******************************************************************************/
/*                 Object Creation via XrdAccAuthorize Object                 */
/******************************************************************************/
  
XrdAccAuthorize *XrdAccAuthorizeObject(XrdOucLogger *lp, const char *cfn)
{
   static XrdOucError Eroute(lp, "acc_");

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
  
XrdAccAccess::XrdAccAccess(XrdOucError *erp)
{
// Get the audit option that we should use
//
   Auditor = XrdAccAuditObject(erp);
}

/******************************************************************************/
/*                                A c c e s s                                 */
/******************************************************************************/
  
const XrdAccPrivs XrdAccAccess::Access(const char *atype,
                                       const char *id,
                                       const char *host,
                                       const char *path, 
                                       const Access_Operation oper)
{
   XrdAccPrivs myprivs;
   char *gname;
   int accok;
   XrdAccGroupList *glp;
   XrdAccPrivCaps caps;
   XrdAccCapability *cp;
   const int plen  = strlen(path);
   const int phash = XrdOucHashVal2(path, plen);
   XrdAccAudit_Options audits = (XrdAccAudit_Options)Auditor->Auditing();
   int isuser = (id && *id && *id != '*' || id[2]);

// Get a shared context for these potentially long running routines
//
   Access_Context.Lock(xs_Shared);

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

// Next add in the group privileges
//
   if (isuser && Atab.G_Hash 
   && (glp = XrdAccConfiguration.GroupMaster.Groups(id)))
      {while(gname = (char *)glp->Next())
            if (cp = Atab.G_Hash->Find((const char *)gname))
               cp->Privs(caps, path, plen, phash);
       delete glp;
      }

// Now add in the netgroup privileges
//
   if (Atab.N_Hash && id && host && 
       (glp = XrdAccConfiguration.GroupMaster.NetGroups(id, host)))
      {while(gname = (char *)glp->Next())
            if (cp = Atab.N_Hash->Find((const char *)gname))
               cp->Privs(caps, path, plen, phash);
       delete glp;
      }

// We are now done with looking at changeable data
//
   Access_Context.UnLock(xs_Shared);


// Compute composite privileges and see if privs need to be returned
//
   myprivs = (XrdAccPrivs)(caps.pprivs & ~caps.nprivs);
   if (!oper) return (const XrdAccPrivs)myprivs;

// Check if auditing is enabled or whether we can do a fastaroo test
//
   if (!audits) return (const XrdAccPrivs)Test(myprivs, oper);
   if ((accok = Test(myprivs, oper)) && !(audits & audit_grant))
      return (const XrdAccPrivs)accok;

// Call the auditing routine and exit
//
   return (const XrdAccPrivs)Audit((const int)accok,atype,id,host,path,oper);
}
  
/******************************************************************************/
/*                                A c c e s s                                 */
/******************************************************************************/
  
const XrdAccPrivs XrdAccAccess::Access(const char *id,
                                       const Access_ID_Type idtype,
                                       const char *path,
                                       const Access_Operation oper)
{
   XrdAccPrivCaps caps;
   XrdAccCapability *cp;
   XrdOucHash<XrdAccCapability> *hp;
   const int plen  = strlen(path);
   const int phash = XrdOucHashVal2(path, plen);

// Select appropriate hash table for the id type
//
   switch(idtype)
        {case AID_Group:      hp = Atab.G_Hash; break;
         case AID_Host:       hp = Atab.H_Hash; break;
         case AID_Netgroup:   hp = Atab.N_Hash; break;
         case AID_Set:        hp = Atab.S_Hash; break;
         case AID_Template:   hp = Atab.T_Hash; break;
         case AID_User:       hp = Atab.U_Hash; break;
         default:             hp = 0;           break;
        }

// Get a shared context while we look up the privileges
//
   Access_Context.Lock(xs_Shared);

// Establish default privileges
//
   if (Atab.Z_List) Atab.Z_List->Privs(caps, path, plen, phash);

// Check for self-describing user template privileges if this is a user
//
   if (idtype == AID_User && Atab.X_List)
      Atab.X_List->Privs(caps, path, plen, phash, id);

// Check for domain privileges if this is a host
//
   if (idtype == AID_Host && Atab.D_List && (cp = Atab.D_List->Find(id)))
      cp->Privs(caps, path, plen, phash, id);

// Look up the specific privileges
//
   if (hp && (cp = hp->Find(id))) cp->Privs(caps, path, plen, phash);

// We are now done with looking at changeable data
//
   Access_Context.UnLock(xs_Shared);

// Perform required access check
//
   if (oper) return (const XrdAccPrivs)Test(
                    (const XrdAccPrivs)(caps.pprivs & ~caps.nprivs), oper);
             return (const XrdAccPrivs)(caps.pprivs & ~caps.nprivs);
}

/******************************************************************************/
/*                                 A u d i t                                  */
/******************************************************************************/
  
const int XrdAccAccess::Audit(const int accok,
                              const char *atype,
                              const char *id,
                              const char *host,
                              const char *path,
                              const Access_Operation oper)
{
// Warning! This table must be in 1-to-1 correspondence with Access_Operation
//
   static char *Opername[] = {(char *)"any",             // 0
                              (char *)"chmod",           // 1
                              (char *)"chown",           // 2
                              (char *)"create",          // 3
                              (char *)"delete",          // 4
                              (char *)"insert",          // 5
                              (char *)"lock",            // 6
                              (char *)"mkdir",           // 7
                              (char *)"read",            // 8
                              (char *)"readdir",         // 9
                              (char *)"rename",          // 10
                              (char *)"stat",            // 10
                              (char *)"update"           // 12
                             };
   const char *opname = (oper > AOP_LastOp ? (char *)"???" : Opername[oper]);

// Route the message appropriately
//
    if (accok) Auditor->Grant(opname, atype, id, host, path);
       else    Auditor->Deny( opname, atype, id, host, path);

// All done, finally
//
   return accok;
}

/******************************************************************************/
/*                                E n a b l e                                 */
/******************************************************************************/
  
void XrdAccAccess::Enable(const char *user, const char *host, 
                          unsigned long oid)
{
   char keybuff[uhoKEYLEN];

// Construct the key
//
   snprintf(keybuff, sizeof(keybuff)-1, "%lx%s%s", oid, user, host);
   keybuff[uhoKEYLEN-1] = '\0';

// Update the key (new or replacement, works regardless)
//
   uhoContext.Lock();
   uho_Hash.Add(keybuff,0,0,(XrdOucHash_Options)(Hash_data_is_key|Hash_count));
   uhoContext.UnLock();
}

/******************************************************************************/
/*                               D i s a b l e                                */
/******************************************************************************/
  
void XrdAccAccess::Disable(const char *user, const char *host, 
                           unsigned long oid)
{
   char keybuff[uhoKEYLEN];

// Construct the key
//
   snprintf(keybuff, sizeof(keybuff)-1, "%lx%s%s", oid, user, host);
   keybuff[uhoKEYLEN-1] = '\0';

// Delete the key, keeping track of multiple additions
//
   uhoContext.Lock();
   uho_Hash.Del(keybuff, Hash_count);
   uhoContext.UnLock();
}

/******************************************************************************/
/*                             i s E n a b l e d                              */
/******************************************************************************/
  
const int XrdAccAccess::isEnabled(const char *user, const char *host, 
                                  unsigned long oid)
{
   char keybuff[uhoKEYLEN], *keydata;

// Construct the key
//
   snprintf(keybuff, sizeof(keybuff)-1, "%lx%s%s", oid, user, host);
   keybuff[uhoKEYLEN-1] = '\0';

// Find the key
//
   uhoContext.Lock();
   keydata = uho_Hash.Find(keybuff);
   uhoContext.UnLock();

// Return fact whether or not we found the entry
//
   return (keydata != 0);
}
  
/******************************************************************************/
/*                              S w a p T a b s                               */
/******************************************************************************/

#define XrdAccSWAP(x) oldtab.x = Atab.x; Atab.x = newtab.x;newtab.x = oldtab.x

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
   XrdAccSWAP(S_Hash);
   XrdAccSWAP(T_Hash);
   XrdAccSWAP(U_Hash);
   XrdAccSWAP(X_List);
   XrdAccSWAP(Z_List);

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

const int XrdAccAccess::Test(const XrdAccPrivs priv,const Access_Operation oper)
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
   return (const int)(need[oper] & priv) == need[oper];
}

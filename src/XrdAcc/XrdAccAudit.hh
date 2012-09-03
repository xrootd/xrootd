#ifndef __ACC_AUDIT__
#define __ACC_AUDIT__
/******************************************************************************/
/*                                                                            */
/*                        X r d A c c A u d i t . h h                         */
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

/******************************************************************************/
/*                         A u d i t _ O p t i o n s                          */
/******************************************************************************/

enum XrdAccAudit_Options {audit_none  = 0,
                          audit_deny  = 1,
                          audit_grant = 2,
                          audit_all   = 3
                         };

/******************************************************************************/
/*                           X r d A c c A u d i t                            */
/******************************************************************************/
  
// This class is really meant to be replaced by anyone who care about auditing.
// Effective auditing is required to meet DOD class C security requirments.

// This class should be placed in a shared library so that an installation can
// easily replace it and routine auditsdits as needed. We supply a brain-dead
// audit that simply issues a message:
//                            deny
// yymmdd hh:mm:ss acc_Audit: grant atype id@host opername path

// Enabling/disabling is done via the method setAudit().

// The external routine XrdAccAuditObject() returns the real audit object
// used by Access(). Developers should derive a class from this class and
// return the object of there choosing up-cast to this object. See the
// routine XrdAccAudit.C for the particulars.

class XrdSysError;

class XrdAccAudit
{
public:

        int Auditing(const XrdAccAudit_Options ops=audit_all)
                    {return auditops & ops;}

virtual void    Deny(const char *opname,
                     const char *tident,
                     const char *atype,
                     const char *id,
                     const char *host,
                     const char *path);

virtual void   Grant(const char *opname,
                     const char *tident,
                     const char *atype,
                     const char *id,
                     const char *host,
                     const char *path);

// setAudit() is used to set the auditing options: audit_none turns audit off
// (the default), audit_deny audit access denials, audit_grant audits access
// grants, and audit_all audits both. See XrdAccAudit.h for more information.
//
void           setAudit(XrdAccAudit_Options aops) {auditops = aops;}

               XrdAccAudit(XrdSysError *erp);
virtual       ~XrdAccAudit() {}

private:

XrdAccAudit_Options auditops;
XrdSysError        *mDest;
};

/******************************************************************************/
/*                    o o a c c _ A u d i t _ O b j e c t                     */
/******************************************************************************/
  
extern XrdAccAudit *XrdAccAuditObject(XrdSysError *erp);

#endif

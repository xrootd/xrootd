/******************************************************************************/
/*                                                                            */
/*                        X r d A c c A u d i t . c c                         */
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

#include <stdio.h>
#include <stdlib.h>

#include "XrdAcc/XrdAccAudit.hh"
#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdAccAudit::XrdAccAudit(XrdSysError *erp)
{

// Set default
//
   auditops = audit_none;
   mDest    = erp;
}

/******************************************************************************/
/*                                  D e n y                                   */
/******************************************************************************/
  
void XrdAccAudit::Deny(const char *opname,
                       const char *tident,
                       const char *atype,
                       const char *id,
                       const char *host,
                       const char *path)
{if (auditops & audit_deny)
    {char buff[2048];
     snprintf(buff, sizeof(buff)-1, "%s deny %s %s@%s %s %s",
              (tident ? tident : ""), atype, id, host, opname, path);
     buff[sizeof(buff)-1] = '\0';
     mDest->Emsg("Audit", buff);
    }
}

/******************************************************************************/
/*                                 G r a n t                                  */
/******************************************************************************/
  
void XrdAccAudit::Grant(const char *opname,
                        const char *tident,
                        const char *atype,
                        const char *id,
                        const char *host,
                        const char *path)
{if (auditops & audit_deny)
    {char buff[2048];
     snprintf(buff, sizeof(buff)-1, "%s grant %s %s@%s %s %s",
              (tident ? tident : ""), atype, id, host, opname, path);
     buff[sizeof(buff)-1] = '\0';
     mDest->Emsg("Audit", buff);
    }
}

/******************************************************************************/
/*                A u d i t   O b j e c t   G e n e r a t o r                 */
/******************************************************************************/
  
XrdAccAudit *XrdAccAuditObject(XrdSysError *erp)
{
static XrdAccAudit AuditObject(erp);

// Simply return the default audit object
//
   return &AuditObject;
}

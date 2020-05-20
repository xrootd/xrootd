#ifndef ___XRD_VOMSFUN_H___
#define ___XRD_VOMSFUN_H___
/******************************************************************************/
/*                                                                            */
/*                         X r d V o m s F u n . h h                          */
/*                                                                            */
/*  (C) 2013  G. Ganis, CERN                                                  */
/*                                                                            */
/*  All rights reserved. The copyright holder's institutional names may not   */
/*  be used to endorse or promote products derived from this software without */
/*  specific prior written permission.                                        */
/*                                                                            */
/*  This file is part of the VOMS extraction XRootD plug-in software suite,   */
/*  here after called VOMS-XRootD (see https://github.com/gganis/voms).       */
/*                                                                            */
/*  VOMS-XRootD is free software: you can redistribute it and/or modify it    */
/*  under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or (at     */
/*  your option) any later version.                                           */
/*                                                                            */
/*  VOMS-XRootD is distributed in the hope that it will be useful, but        */
/*  WITHOUT ANY WARRANTY, not even the implied warranty of MERCHANTABILITY or */
/*  FITNESS FOR A PARTICULAR PURPOSE.                                         */
/*  See the GNU Lesser General Public License for more details.               */
/*                                                                            */
/*  You should have received a copy of the GNU Lesser General Public License  */
/*  along with VOMS-XRootD in a file called COPYING.LGPL (LGPL license) and   */
/*  file COPYING (GPL license). If not, see <http://www.gnu.org/licenses/>.   */
/*                                                                            */
/******************************************************************************/
  
#include "openssl/x509.h"
#include "openssl/pem.h"

#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucString.hh"

class XrdSecEntity;
class XrdSysError;
class XrdSysLogger;

class XrdVomsFun
{
public:

// Supported cert formats
//
enum CertFormat
    {gCertRaw  = 0,
     gCertPEM  = 1,
     gCertX509 = 2
    };

void SetCertFmt(CertFormat n) {gCertFmt = n;}

int  VOMSFun(XrdSecEntity &ent);

int  VOMSInit(const char *cfg);

     XrdVomsFun(XrdSysError &erp);

    ~XrdVomsFun() {} // Once constructed never deleted (except for Http).

private:

void FmtExtract(XrdOucString &out, XrdOucString in, const char *tag);
void NameOneLine(X509_NAME *nm, XrdOucString &s);
void FmtReplace(XrdSecEntity &ent);

// These settings are configurable
//

CertFormat      gCertFmt;         // certfmt: see constructor
short           gGrpWhich;        // grpopt's which = 0|1|2 [2]
short           gDebug;           // Verbosity control 0 | 1 | 2
XrdOucHash<int> gGrps;            // hash table with grps=grp1[,grp2,...]
XrdOucHash<int> gVOs;             // hash table with vos=vo1[,vo2,...]
XrdOucString    gRequire;         // String with configuration options use to:
XrdOucString    gGrpFmt;          // format contents of XrdSecEntity::grps
XrdOucString    gRoleFmt;         // format contents of XrdSecEntity::role
XrdOucString    gVoFmt;           // format contents of XrdSecEntity::vorg

XrdSysError    &gDest;
XrdSysLogger   *gLogger;
};
#endif

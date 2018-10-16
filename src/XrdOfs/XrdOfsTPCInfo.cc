/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C I n f o . c c                       */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <string.h>
#include <strings.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdOfs/XrdOfsStats.hh"
#include "XrdOfs/XrdOfsTPCInfo.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdSysError  OfsEroute;
extern XrdOfsStats  OfsStats;
extern XrdOss      *XrdOfsOss;

namespace XrdOfsTPCParms
{
extern bool         autoRM;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOfsTPCInfo::~XrdOfsTPCInfo()
{
// Check if we should remove the file
//
   if (isDST && !isAOK && XrdOfsTPCParms::autoRM && Lfn)
      XrdOfsOss->Unlink(Lfn);

// Delete all appendages
//
   if (Key) {free(Key); Key = 0;}
   if (Org) {free(Org); Org = 0;}
   if (Lfn) {free(Lfn); Lfn = 0;}
   if (Dst) {free(Dst); Dst = 0;}
   if (Spr) {free(Spr); Spr = 0;}
   if (Tpr) {free(Tpr); Tpr = 0;}
   if (Cks) {free(Cks); Cks = 0;}
   if (Crd) {free(Crd); Crd = 0; Csz = 0;}
   if (cbP) delete cbP;
}
  
/******************************************************************************/
/*                                  F a i l                                   */
/******************************************************************************/
  
int XrdOfsTPCInfo::Fail(XrdOucErrInfo *eRR, const char *eMsg, int eCode)
{
   char Buff[2048];

// Format the error message
//
    snprintf(Buff, sizeof(Buff), "Unable to copy %s; %s", Lfn, eMsg);

// Print it out if debugging is enabled
//
#ifndef NODEBUG
   OfsEroute.Emsg("TPC", Org, Buff);
#endif

// Place the error message in the error object and return
//
   if (eRR) eRR->setErrInfo(eCode, Buff);
   OfsStats.Add(OfsStats.Data.numTPCerrs);
   return SFS_ERROR;
}

/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/
  
int XrdOfsTPCInfo::Match(const char *cKey, const char *cOrg,
                         const char *xLfn, const char *xDst)
{
    if (cKey) {if (!Key || strcmp(Key, cKey)) return 0;}
       else if (Key) return 0;

    if (cOrg) {if (!Org || strcmp(Org, cOrg)) return 0;}
       else if (Org) return 0;

    if (xLfn) {if (!Lfn || strcmp(Lfn, xLfn)) return 0;}
       else if (Lfn) return 0;

    if (xDst) {if (!Dst || strcmp(Dst, xDst)) return 0;}
       else if (Dst) return 0;

    return 1;
}

/******************************************************************************/
/*                                 R e p l y                                  */
/******************************************************************************/
  
void XrdOfsTPCInfo:: Reply(int rC, int eC, const char *eMsg, XrdSysMutex *mP)
{
   XrdOucCallBack *myCB = cbP;

// Clear pointer to call back prior to unlocking any locks
//
   cbP = 0;

// Make sure a reply is valid here (i.e. client is in waitresop). If not,
// then we need to scuttle the whole shebang (must be done with a lock).
//
   if (inWtR)
      {inWtR = false;
       if (mP) mP->UnLock();
       if (myCB) myCB->Reply(rC, eC, eMsg, Lfn);
      } else if (mP) mP->UnLock();
   delete myCB;
}

/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
const char *XrdOfsTPCInfo::Set(const char *cKey, const char *cOrg,
                               const char *xLfn, const char *xDst,
                               const char *xCks)
{
   const char *etext;

// Set the key
//
   if (Key) free(Key);
   Key = (cKey ? strdup(cKey) : 0);

// Set the origin
//
   if (Org) free(Org);
   Org = (cOrg ? strdup(cOrg) : 0);

// Set the lfn
//
   if (Lfn) free(Lfn);
   Lfn = (xLfn ? strdup(xLfn) : 0);

// Set optional dst
//
   if (Dst) {free(Dst); Dst = 0;}
   if (xDst)
      {XrdNetAddr dAddr;
       if (!(etext = dAddr.Set(xDst,0)))
          Dst = strdup(dAddr.Name("0.0.0.0",&etext));
       if (etext) return etext;
      }

// Set the cks
//
   if (Cks) free(Cks);
   Cks = (xCks ? strdup(xCks) : 0);

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                 S e t C B                                  */
/******************************************************************************/
  
int XrdOfsTPCInfo::SetCB(XrdOucErrInfo *eRR)
{
   if (cbP) delete cbP;
   cbP = new XrdOucCallBack();
   if ((cbP->Init(eRR))) return 0;
   delete cbP; cbP = 0;
   return Fail(eRR, "tpc callback logic error", EPROTO);
}

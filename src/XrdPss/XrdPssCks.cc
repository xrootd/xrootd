/******************************************************************************/
/*                                                                            */
/*                          X r d P s s C k s . c c                           */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdPss/XrdPss.hh"
#include "XrdPss/XrdPssCks.hh"

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
  
/******************************************************************************/
/*                            X r d C k s I n i t                             */
/******************************************************************************/

XrdVERSIONINFO(XrdCksInit,PssCks);
  
// Return the proxy checksum object created by the storage system interface.
//
extern "C"
{XrdCks *XrdCksInit(XrdSysError *eDest,  // The error msg object
                    const char  *cFN,    // Config file name
                    const char  *Parms   // Parms via lib directive
                   )  {return (XrdCks *)new XrdPssCks(eDest);}
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdPssCks::XrdPssCks(XrdSysError *erP) : XrdCks(erP)
{

// Prefill the native digests we support
//
   csTab[0].Len =  4; strcpy(csTab[0].Name, "adler32");
   csTab[1].Len =  4; strcpy(csTab[1].Name, "crc32");
   csTab[2].Len = 16; strcpy(csTab[2].Name, "md5");
   csLast = 2;
}

/******************************************************************************/
/* Private:                         F i n d                                   */
/******************************************************************************/
  
XrdPssCks::csInfo *XrdPssCks::Find(const char *Name)
{
   int i;
   for (i = 0; i <= csLast; i++)
       if (!strcmp(Name, csTab[i].Name)) return &csTab[i];
   return 0;
}
  
/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

int XrdPssCks::Get(const char *Pfn, XrdCksData &Cks)
{
   static const int cksBLen = 256;
   char             cksBuff[cksBLen], pBuff[2048], cgiBuff[32], *tP;
   XrdOucTokenizer  Resp(cksBuff);
   time_t           Mtime;
   int              rc, n;

// Construct the cgi for digest type
//
   n = snprintf(cgiBuff, sizeof(cgiBuff), "cks.type=%s", Cks.Name);

// Direct the path to the origin (we don't have any cgi or ident info)
//
   if (!XrdPssSys::P2URL(rc, pBuff, sizeof(pBuff), Pfn, 0, cgiBuff, n, 0, 0))
      return rc;

// First step is to getthe checksum value
//
   if ((rc = XrdPosixXrootd::QueryChksum(pBuff, Mtime, cksBuff, cksBLen)) <= 0)
      return (rc ? -errno : -ENOTSUP);

// Get the checksum name
//
   if (!Resp.GetLine() || !(tP = Resp.GetToken()) || !(*tP)) return -ENOMSG;
   if (!Cks.Set(tP)) return -ENOTSUP;

// Now get the checksum value
//
   if (!(tP = Resp.GetToken()) || !(*tP)) return -ENODATA;
   if (!Cks.Set(tP, strlen(tP))) return -ENOTSUP;

// Set remaining fields and return success
//
   Cks.fmTime = static_cast<long long>(Mtime);
   Cks.csTime = 0;
   return Cks.Length;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdPssCks::Init(const char *ConfigFN, const char *DfltCalc)
{
   int i;

// See if we need to set the default calculation
//
   if (DfltCalc)
      {for (i = 0; i < csLast; i++) if (!strcmp(csTab[i].Name, DfltCalc)) break;
       if (i >= csMax)
          {eDest->Emsg("Config", DfltCalc, "cannot be made the default; "
                                           "not supported.");
           return 0;
          }
       if (i) {csInfo Temp = csTab[i]; csTab[i] = csTab[0]; csTab[0] = Temp;}
      }

// All done
//
   return 1;
}

/******************************************************************************/
/*                                  N a m e                                   */
/******************************************************************************/
  
const char *XrdPssCks::Name(int seqNum)
{

   return (seqNum < 0 || seqNum > csLast ? 0 : csTab[seqNum].Name);
}

/******************************************************************************/
/*                                  S i z e                                   */
/******************************************************************************/
  
int XrdPssCks::Size(const char *Name)
{
   csInfo *iP = (Name != 0 ? Find(Name) : &csTab[0]);
   return (iP != 0 ? iP->Len : 0);
}

/******************************************************************************/
/*                                   V e r                                    */
/******************************************************************************/

int XrdPssCks::Ver(const char *Pfn, XrdCksData &Cks)
{
   XrdCksData fCks;
   csInfo *csIP = &csTab[0];
   int rc;

// Determine which checksum to get
//
   if (!(*Cks.Name)) strcpy(Cks.Name, csTab[0].Name);
      else if (!(csIP = Find(Cks.Name))) return -ENOTSUP;

// Get the file checksum
//
   if ((rc = Get(Pfn, fCks))) return rc;

// Compare the checksums
//
   return (!strcmp(fCks.Name, Cks.Name) && fCks.Length == Cks.Length
       &&  !memcmp(fCks.Value, Cks.Value, csIP->Len));
}

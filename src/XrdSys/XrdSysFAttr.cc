/******************************************************************************/
/*                                                                            */
/*                        X r d S y s F A t t r . c c                         */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"

/******************************************************************************/
/*                 P l a t f o r m   D e p e n d e n c i e s                  */
/******************************************************************************/

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
namespace
{
XrdSysFAttr  dfltXAttr;
}

XrdSysXAttr *XrdSysFAttr::Xat = &dfltXAttr;

/******************************************************************************/
/*            X r d S y s F A t t r   I m p l e m e n t a t i o n             */
/******************************************************************************/
  
#if    defined(__FreeBSD__)
#include "XrdSys/XrdSysFAttrBsd.icc"
#elif defined(__linux__)
#include "XrdSys/XrdSysFAttrLnx.icc"
#elif defined(__APPLE__)
#include "XrdSys/XrdSysFAttrMac.icc"
#elif defined(__solaris__)
#include "XrdSys/XrdSysFAttrSun.icc"
#else
int XrdSysFAttr::Del(const char *Aname, const char *Path)
                {return -ENOTSUP;}
int XrdSysFAttr::Del(const char *Aname, int fd)
                {return -ENOTSUP;}
int XrdSysFAttr::Get(const char *Aname, void *Aval, int Avsz, const char *Path)
                {return -ENOTSUP;}
int XrdSysFAttr::Get(const char *Aname, void *Aval, int Avsz, int fd)
                {return -ENOTSUP;}
int XrdSysFAttr::Set(const char *Aname, const void *Aval, int Avsz,
                     const char *Path,  int isNew)
                {return -ENOTSUP;}
int XrdSysFAttr::Set(const char *Aname, const void *Aval, int Avsz,
                     int         fd,    int isNew)
                {return -ENOTSUP;}
int XrdSysFAttr::Set(XrdSysError *erp) {return 0;}
#endif

/******************************************************************************/
/*                              D i a g n o s e                               */
/******************************************************************************/
  
int XrdSysFAttr::Diagnose(const char *Op, const char *Var,
                          const char *Path,  int ec)
{
   char buff[512];

// Screen out common case
//
   if (ec == ENOATTR || ec == ENOENT) return -ENOENT;

// Format message insert and print if we can actually say anything
//
   if (Say)
      {snprintf(buff, sizeof(buff), "%s attr %s from", Op, Var);
       Say->Emsg("FAttr", ec, buff, Path);
      }

// Return negative code
//
   return -ec;
}
  
/******************************************************************************/
/*                                  F r e e                                   */
/******************************************************************************/

void XrdSysFAttr::Free(XrdSysFAttr::AList *aLP)
{
   AList *aNP;

// Free all teh structs using free as they were allocated using malloc()
//
   while(aLP) {aNP = aLP->Next; free(aLP); aLP = aNP;}
}

/******************************************************************************/
/*                                g e t E n t                                 */
/******************************************************************************/
  
XrdSysFAttr::AList *XrdSysFAttr::getEnt(const char *Path,  int fd,
                                        const char *Aname,
                                        XrdSysFAttr::AList *aP, int *msP)
{
   AList *aNew;
   int sz = 0, n = strlen(Aname);

// Get the data size of this attribute if so wanted
//
   if (!n || (msP && !(sz = Get(Aname, 0, 0, Path, fd)))) return 0;

// Allocate a new dynamic struct
//
   if (!(aNew = (AList *)malloc(sizeof(AList) + n))) return 0;

// Initialize the structure
//
   aNew->Next = aP;
   aNew->Vlen = sz;
   aNew->Nlen = n;
   strcpy(aNew->Name, Aname); // Gauranteed to fit

// All done
//
   if (msP && *msP < sz) *msP = sz;
   return aNew;
}

/******************************************************************************/
/*                             S e t P l u g i n                              */
/******************************************************************************/
  
void XrdSysFAttr::SetPlugin(XrdSysXAttr *xaP)
{
   if (Xat && Xat != &dfltXAttr) delete Xat;
   Xat = xaP;
}

#ifndef __ACC_CAPABILITY__
#define __ACC_CAPABILITY__
/******************************************************************************/
/*                                                                            */
/*                   X r d A c c C a p a b i l i t y . h h                    */
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "XrdAcc/XrdAccPrivs.hh"

/******************************************************************************/
/*                      X r d A c c C a p a b i l i t y                       */
/******************************************************************************/
  
class XrdAccCapability
{
public:
void                Add(XrdAccCapability *newcap) {next = newcap;}

XrdAccCapability   *Next() {return next;}

// Privs() searches the associated capability for a prefix matching path. If one
// is found, the privileges are or'd into the passed XrdAccPrivCaps struct and
// a 1 is returned. Otherwise, 0 is returned and XrdAccPrivCaps is unchanged.
//
int                 Privs(      XrdAccPrivCaps &pathpriv,
                          const char           *pathname,
                          const int             pathlen,
                          const unsigned long   pathhash,
                          const char           *pathsub=0);

int                 Privs(      XrdAccPrivCaps &pathpriv,
                          const char           *pathname,
                          const int             pathlen,
                          const char           *pathsub=0)
                          {extern unsigned long XrdOucHashVal2(const char *,int);
                           return Privs(pathpriv, pathname, pathlen,
                                  XrdOucHashVal2(pathname,(int)pathlen),pathsub);}

int                 Privs(      XrdAccPrivCaps &pathpriv,
                          const char           *pathname,
                          const char           *pathsub=0)
                          {extern unsigned long XrdOucHashVal2(const char *,int);
                           int pathlen = strlen(pathname);
                           return Privs(pathpriv, pathname, pathlen,
                                  XrdOucHashVal2(pathname, pathlen), pathsub);}

int                 Subcomp(const char *pathname, const int pathlen,
                            const char *pathsub,  const int sublen);

                  XrdAccCapability(char *pathval, XrdAccPrivCaps &privval);

                  XrdAccCapability(XrdAccCapability *taddr)
                        {next = 0; ctmp = taddr;
                         pkey = 0; path = 0; plen = 0; pins = 0; prem = 0;
                        }

                 ~XrdAccCapability();
private:
XrdAccCapability *next;      // -> Next capability
XrdAccCapability *ctmp;      // -> Capability template

/*----------- The below fields are valid when template is zero -----------*/

XrdAccPrivCaps   priv;
unsigned long    pkey;
char            *path;
int              plen;
int              pins;    // index of @=
int              prem;    // remaining length after @=
};

/******************************************************************************/
/*                         X r d A c c C a p N a m e                          */
/******************************************************************************/

class XrdAccCapName
{
public:
void              Add(XrdAccCapName *cnp) {next = cnp;}

XrdAccCapability *Find(const char *name);

       XrdAccCapName(char *name, XrdAccCapability *cap)
                    {next = 0; CapName = strdup(name); CNlen = strlen(name);
                     C_List = cap;
                    }
      ~XrdAccCapName();
private:
XrdAccCapName    *next;
char             *CapName;
int               CNlen;
XrdAccCapability *C_List;
};
#endif

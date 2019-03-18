/******************************************************************************/
/*                                                                            */
/*                          X r d C p F i l e . c c                           */
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
  
#include "XrdApps/XrdCpFile.hh"
#include "XrdOuc/XrdOucNSWalk.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
const char *XrdCpFile::mPfx = 0;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCpFile::XrdCpFile(const char *FSpec, int &badURL)
{
   static struct proto {const char *pHdr; int pHsz; PType pVal;}
                 pTab[] = {{"xroot://", 8, isXroot},
                           { "root://", 7, isXroot},
                           { "http://", 7, isHttp},
                           {"https://", 8, isHttps}
                          };
   static int pTnum = sizeof(pTab)/sizeof(struct proto);
   const char *Slash;
   int i;

// Do some common initialization
//
   Doff  = 0;
   Dlen  = 0;
   Next  = 0;
   fSize = 0;
   badURL= 0;
   memset(ProtName, 0, sizeof(ProtName));

// Copy out the path and remove trailing slashes (except the last one)
//
   Path = strdup(FSpec);
   i = strlen(Path);
   while(i) if (Path[i-1] != '/' || (i > 1 && Path[i-2] != '/')) break;
               else Path[--i] = 0;

// Check for stdin stdout spec
//
   if (!strcmp(Path, "-"))
      {Protocol = isStdIO;
       return;
      }

// Dtermine protocol of the incomming spec
//
   for (i = 0; i < pTnum; i++)
       {if (!strncmp(FSpec, pTab[i].pHdr, pTab[i].pHsz))
           {Protocol = pTab[i].pVal;
            memcpy(ProtName, pTab[i].pHdr, pTab[i].pHsz-3);
            return;
           }
       }

// See if this is a file
//
   Protocol = isFile;
   if (!strncmp(Path, "file://", 7))
      {char *pP = Path + 7;
               if (!strncmp(pP, "localhost", 9)) memmove( Path, pP + 9, strlen( pP + 9 ) + 1 );
          else if (*pP == '/') memmove( Path, pP, strlen( pP ) + 1 );
          else {Protocol = isOther;
                strcpy(ProtName, "remote");
                return;
               }
      }

// Set the default Doff and Dlen assuming non-recursive copy
//
   if ((Slash = rindex(Path, '/'))) Dlen = Doff = Slash - Path + 1;
}

/******************************************************************************/

XrdCpFile::XrdCpFile(char *FSpec, struct stat &Stat, short doff, short dlen)
                    : Next(0), Path(FSpec), Doff(doff), Dlen(dlen),
                      Protocol(isFile), fSize(Stat.st_size)
                    {strcpy(ProtName, "file");}

/******************************************************************************/
/*                                E x t e n d                                 */
/******************************************************************************/
  
int XrdCpFile::Extend(XrdCpFile **pLast, int &nFile, long long &nBytes)
{
   XrdOucNSWalk nsObj(0, Path, 0, XrdOucNSWalk::retFile|XrdOucNSWalk::Recurse);
   XrdOucNSWalk::NSEnt *nP, *nnP;
   XrdCpFile *fP, *pP = this;
   int rc;
   short dlen, doff = strlen(Path);

   nsObj.setMsgOn(mPfx);

   while((nP = nsObj.Index(rc)) && rc == 0)
        {do {dlen = nP->Plen - doff;
             fP = new XrdCpFile(nP->Path, nP->Stat, doff, dlen);
             nFile++; nBytes += nP->Stat.st_size; nP->Path = 0;
             pP->Next = fP; pP = fP;
             nnP = nP->Next; delete nP;
            } while((nP = nnP));
        }

   if (pLast) *pLast = pP;
   return rc;
}

/******************************************************************************/
/*                               R e s o l v e                                */
/******************************************************************************/

int XrdCpFile::Resolve()
{
   struct stat Stat;

// Ignore this call if this is not a file
//
   if (Protocol != isFile) return 0;

// This should exist but it might not, the caller will determine what to do
//
   if (stat(Path, &Stat)) return errno;

// Find out what this really is
//
        if (S_ISREG(Stat.st_mode)) fSize = Stat.st_size;
   else if (S_ISDIR(Stat.st_mode))      Protocol = isDir;
   else if (!strcmp(Path, "/dev/null")) Protocol = isDevNull;
   else if (!strcmp(Path, "/dev/zero")) Protocol = isDevZero;
   else return ENOTSUP;

// All is well
//
   return 0;
}

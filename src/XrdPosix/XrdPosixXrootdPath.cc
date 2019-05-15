/******************************************************************************/
/*                                                                            */
/*                 X r d P o s i x X r o o t d P a t h . c c                  */
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>

#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdPosix/XrdPosixTrace.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/

namespace
{
struct ProtoTable
      {const char *name;
       int         nlen;
      };

static const int ptEnts = 8; // Number of protocol entries we support
}

namespace XrdPosixGlobals
{
extern XrdOucName2Name *theN2N;
extern bool             oidsOK;

       ProtoTable       protoTab[ptEnts] = {{"root://", 7}, {"xroot://", 8}};
}
  
/******************************************************************************/
/*         X r d P o s i x X r o o t P a t h   C o n s t r u c t o r          */
/******************************************************************************/

XrdPosixXrootPath::XrdPosixXrootPath()
    : xplist(0),
      pBase(0)
{
   XrdOucTokenizer thePaths(0);
   char *plist = 0, *colon = 0, *subs = 0, *lp = 0, *tp = 0;
   int aOK = 0;

   cwdPath = 0; cwdPlen = 0;

   if (!(plist = getenv("XROOTD_VMP")) || !*plist) return;
   pBase = strdup(plist);

   thePaths.Attach(pBase);

   if ((lp = thePaths.GetLine())) while((tp = thePaths.GetToken()))
      {aOK = 1;
       if ((colon = rindex(tp, (int)':')) && *(colon+1) == '/')
          {if (!(subs = index(colon, (int)'='))) subs = 0;
              else if (*(subs+1) == '/') {*subs = '\0'; subs++;}
                      else if (*(subs+1)) aOK = 0;
                              else {*subs = '\0'; subs = (char*)"";}
          } else aOK = 0;

       if (aOK)
          {*colon++ = '\0';
           while(*(colon+1) == '/') colon++;
           xplist = new xpath(xplist, tp, colon, subs);
          } else DMSG("Path", "Invalid XROOTD_VMP token '" <<tp <<'"');
      }
}

/******************************************************************************/
/*          X r d P o s i x X r o o t P a t h   D e s t r u c t o r           */
/******************************************************************************/
  
XrdPosixXrootPath::~XrdPosixXrootPath()
{
   struct xpath *xpnow;

   while((xpnow = xplist))
        {xplist = xplist->next; delete xpnow;}
}
  
/******************************************************************************/
/*                X r d P o s i x P a t h : : A d d P r o t o                 */
/******************************************************************************/

bool XrdPosixXrootPath::AddProto(const char *proto)
{
   int i;

// Check if we already have this protocol entry. The proto argument must be
// in the form "pname://", where pname is the protocol name.
//
   for (i = 0; i < ptEnts && XrdPosixGlobals::protoTab[i].name; i++)
       if (!strcmp(proto, XrdPosixGlobals::protoTab[i].name)) return true;

// Add the entry if we have room
//
   if (i >= ptEnts) return false;
   XrdPosixGlobals::protoTab[i].name = strdup(proto);
   XrdPosixGlobals::protoTab[i].nlen = strlen(proto);
   return true;
}
  
/******************************************************************************/
/*                     X r d P o s i x P a t h : : C W D                      */
/******************************************************************************/
  
void XrdPosixXrootPath::CWD(const char *path)
{
   if (cwdPath) free(cwdPath);
   cwdPlen = strlen(path);
   if (*(path+cwdPlen-1) == '/') cwdPath = strdup(path);
      else if (cwdPlen <= MAXPATHLEN)
           {char buff[MAXPATHLEN+8];
            strcpy(buff, path); 
            *(buff+cwdPlen  ) = '/';
            *(buff+cwdPlen+1) = '\0';
            cwdPath = strdup(buff); cwdPlen++;
           }
}

/******************************************************************************/
/*                     X r d P o s i x P a t h : : P 2 L                      */
/******************************************************************************/

const char *XrdPosixXrootPath::P2L(const char  *who,
                                   const char  *inP,
                                         char *&relP,
                                         bool   ponly)
{
   EPNAME("P2L");
   const char *urlP, *slash, *quest;
   char *outP, *lfnP, pfnBuff[1032], lfnBuff[1032];
   int cgiLen, lfnLen, pfnLen, pfxLen, n;
   bool notOurs = true;

// Preset repP to zero to indicate no translation required, nothing to free
//
   relP = 0;

// Check if we need to do any translation at all
//
   if (!XrdPosixGlobals::theN2N && !ponly) return inP;

// If this is a protocol we support, then we can convert the path
//
   for (int i = 0; i < ptEnts && XrdPosixGlobals::protoTab[i].name; i++)
       if (!strncmp(inP, XrdPosixGlobals::protoTab[i].name,
                         XrdPosixGlobals::protoTab[i].nlen))
          {urlP = inP + XrdPosixGlobals::protoTab[i].nlen;
           notOurs = false;
           break;
          }
   if (notOurs) return inP;

// Search for the next slash which must be followed by another slash unless we
// are allowing object ids.
//
   if (!(slash = index(urlP, '/'))) return inP;
   if (*(slash+1) != '/' && !XrdPosixGlobals::oidsOK) return inP;
   slash++;
   pfxLen = slash - inP;

// Search for start of the cgi
//
   if ((quest = index(slash, '?')))
      {cgiLen = strlen(quest);
       pfnLen = quest - slash;
      } else {
       cgiLen = 0;
       pfnLen = strlen(slash);
      }

// Copy out the pfn. It must fit our buffer
//
   if (pfnLen >= (int)sizeof(pfnBuff))
      {errno = ENAMETOOLONG;
       return 0;
      }
   strncpy(pfnBuff, slash, pfnLen);
   *(pfnBuff+pfnLen) = 0;

// Invoke the name2name translator if we have one
//
   if (XrdPosixGlobals::theN2N)
      {if ((n = XrdPosixGlobals::theN2N->pfn2lfn(pfnBuff,lfnBuff,sizeof(lfnBuff))))
          {errno = n;
           return 0;
          }
       lfnP = lfnBuff;
      } else lfnP = pfnBuff;

// If only the path is wanted, then adjust lengths
//
   if (ponly) pfxLen = cgiLen = 0;

// Allocate storage to assemble the new url
//
   lfnLen = strlen(lfnP);
   if (!(relP = (char *)malloc(pfxLen + lfnLen + cgiLen + 1)))
      {errno = ENOMEM;
       return 0;
      }
   outP = relP;

// Assemble the new url, we know we have room to do this
//
   if (pfxLen) {strncpy(outP, inP, pfxLen); outP += pfxLen;}
   strcpy( outP, lfnP);
   if (cgiLen) strcpy(outP+lfnLen, quest);

// Do some debugging
//
   DEBUG(who <<' ' <<pfnBuff <<" pfn2lfn " <<lfnBuff);

// All done, return result
//
   return relP;
}
  
/******************************************************************************/
/*                     X r d P o s i x P a t h : : U R L                      */
/******************************************************************************/
  
char *XrdPosixXrootPath::URL(const char *path, char *buff, int blen)
{
   struct xpath *xpnow = xplist;
   char tmpbuff[2048];
   int plen, pathlen = 0;

// Check if this is a protocol we support
//
   for (int i = 0; i < ptEnts && XrdPosixGlobals::protoTab[i].name; i++)
      {if (!strncmp(path, XrdPosixGlobals::protoTab[i].name,
                          XrdPosixGlobals::protoTab[i].nlen))
          return (char *)path;
     }

// If a relative path was specified, convert it to an absolute path
//
   if (path[0] == '.' && path[1] == '/' && cwdPath)
      {pathlen = (strlen(path) + cwdPlen - 2);
       if (pathlen < (int)sizeof(tmpbuff))
          {strcpy(tmpbuff, cwdPath);
           strcpy(tmpbuff+cwdPlen, path+2);
           path = (const char *)tmpbuff;
          }  else return 0;
      }

// Check if this path starts with one or our known paths
//
   while(*(path+1) == '/') path++;
   while(xpnow)
        if (!strncmp(path, xpnow->path, xpnow->plen)) break;
           else xpnow = xpnow->next;

// If we did not match a path, this is not our path.
//
   if (!xpnow) return 0;
   if (!buff) return (char *)1;

// Verify that we won't overflow the buffer
//
   if (!pathlen) pathlen = strlen(path);
   plen = pathlen + xpnow->servln + 2;
   if (xpnow->nath) plen =  plen - xpnow->plen + xpnow->nlen;
   if (plen >= blen) return 0;

// Build the url
//
   strcpy(buff, XrdPosixGlobals::protoTab[0].name);
   strcat(buff, xpnow->server);
   strcat(buff, "/");
   if (xpnow->nath) {strcat(buff, xpnow->nath); path += xpnow->plen;}
   if (*path != '/') strcat(buff, "/");
   strcat(buff, path);
   return buff;
}

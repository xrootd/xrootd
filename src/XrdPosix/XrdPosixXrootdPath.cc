/******************************************************************************/
/*                                                                            */
/*                 X r d P o s i x X r o o t d P a t h . c c                  */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>

#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"
#include "XrdSys/XrdSysHeaders.hh"
  
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
          } else cerr <<"XrdPosix: Invalid XROOTD_VMP token '" <<tp <<'"' <<endl;
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
/*                     X r d P o s i x P a t h : : U R L                      */
/******************************************************************************/
  
char *XrdPosixXrootPath::URL(const char *path, char *buff, int blen)
{
   const char   *rproto = "root://";
   const int     rprlen = strlen(rproto);
   const char   *xproto = "xroot://";
   const int     xprlen = strlen(xproto);
   struct xpath *xpnow = xplist;
   char tmpbuff[2048];
   int plen, pathlen = 0;

// If this starts with 'root", then this is our path
//
   if (!strncmp(rproto, path, rprlen)) return (char *)path;

// If it starts with xroot, then convert it to be root
//
   if (!strncmp(xproto, path, xprlen))
      {if (!buff) return (char *)1;
       if ((int(strlen(path))) > blen) return 0;
       strcpy(buff, path+1);
       return buff;
      }

// If a relative path was specified, convert it to an abso9lute path
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
   plen = xprlen + pathlen + xpnow->servln + 2;
   if (xpnow->nath) plen =  plen - xpnow->plen + xpnow->nlen;
   if (plen >= blen) return 0;

// Build the url
//
   strcpy(buff, rproto);
   strcat(buff, xpnow->server);
   strcat(buff, "/");
   if (xpnow->nath) {strcat(buff, xpnow->nath); path += xpnow->plen;}
   if (*path != '/') strcat(buff, "/");
   strcat(buff, path);
   return buff;
}

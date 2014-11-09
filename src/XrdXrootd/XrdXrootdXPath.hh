#ifndef __XROOTD_XPATH__
#define __XROOTD_XPATH__
/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d X P a t h . h h                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <strings.h>
#include <stdlib.h>

#define XROOTDXP_OK        1
#define XROOTDXP_NOLK      2
#define XROOTDXP_NOCGI     4
#define XROOTDXP_NOSLASH   8
  
class XrdXrootdXPath
{
public:

inline XrdXrootdXPath *Next()  {return next;}
inline int             Opts()  {return pathopt;}
inline char           *Path()  {return path;}
inline char           *Path(int &PLen)
                               {PLen = pathlen; return path;}
       void            Set(int opts, const char *pathdata=0)
                          {pathopt = opts;
                           if (pathdata)
                              {if (path) free(path);
                               pathlen = strlen(pathdata);
                               path    = strdup(pathdata);
                              }
                          }

       void            Insert(const char *pd, int popt=0, int flags=XROOTDXP_OK)
                             {XrdXrootdXPath *pp = 0, *p = next;
                              XrdXrootdXPath *newp = new XrdXrootdXPath(pd,popt,flags);
                              while(p && newp->pathlen >= p->pathlen)
                                   {pp = p; p = p->next;}
                              newp->next = p;
                              if (pp) pp->next = newp;
                                 else     next = newp;
                             }

inline int             Validate(const char *pd, const int pl=0)
                               {int plen = (pl ? pl : strlen(pd));
                                XrdXrootdXPath *p = next;
                                while(p && plen >= p->pathlen)
                                     {if (!strncmp(pd, p->path, p->pathlen))
                                         return p->pathopt;
                                      p=p->next;
                                     }
                                return 0;
                               }

       XrdXrootdXPath(const char *pathdata="",int popt=0,int flags=XROOTDXP_OK)
                     {next = 0;
                      pathopt = popt | flags;
                      pathlen = strlen(pathdata);
                      path    = strdup(pathdata);
                     }

      ~XrdXrootdXPath() {if (path) free(path);}

private:

       XrdXrootdXPath *next;
       int             pathlen;
       int             pathopt;
       char           *path;
};
#endif

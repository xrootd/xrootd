#ifndef __XROOTD_XPATH__
#define __XROOTD_XPATH__
/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d X P a t h . h h                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <strings.h>
#include <stdlib.h>
  
class XrdXrootdXPath
{
public:

inline XrdXrootdXPath *First() {return first;}
inline XrdXrootdXPath *Next()  {return next;}
inline char           *Path()  {return path;}

static void            Insert(char *pd)
                             {XrdXrootdXPath *pp = 0, *p = first;
                              XrdXrootdXPath *newp = new XrdXrootdXPath(pd);
                              while(p && newp->pathlen >= p->pathlen)
                                   {pp = p; p = p->next;}
                              newp->next = p;
                              if (pp) pp->next = newp;
                                 else first    = newp;
                             }

inline int             Validate(const char *pd, const int pl=0)
                               {int plen = (pl ? pl : strlen(pd));
                                XrdXrootdXPath *p = first;
                                while(p && plen >= p->pathlen)
                                     {if (!strncmp(pd, p->path, p->pathlen))
                                         return 1;
                                      p=p->next;
                                     }
                                return 0;
                               }

       XrdXrootdXPath(char *pathdata=(char *)"")
                     {next = 0;
                      pathlen = strlen(pathdata);
                      path    = strdup(pathdata);
                     }

      ~XrdXrootdXPath() {if (path) free(path);}

private:

static XrdXrootdXPath *first;
       XrdXrootdXPath *next;
       int             pathlen;
       char           *path;
};
#endif

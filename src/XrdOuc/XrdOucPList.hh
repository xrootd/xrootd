#ifndef __OUC_PLIST__
#define __OUC_PLIST__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c P L i s t . h h                         */
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
  
class XrdOucPList
{
public:

inline int                 Attr() {return attrs;}
inline unsigned long long  Flag() {return flags;}
inline const char         *Name() {return name;}
inline XrdOucPList        *Next() {return next;}
inline char               *Path() {return path;}
inline int                 Plen() {return pathlen;}

inline int          PathOK(const char *pd, const int pl)
                          {return pl >= pathlen && !strncmp(pd, path, pathlen);}

inline void         Set(int                aval) {attrs = aval;}
inline void         Set(unsigned long long fval) {flags = fval;}
inline void         Set(const char *pd, const char *pn)
                       {if (path) free(path);
                        pathlen = strlen(pd);
                        int n = strlen(pn) + 1 + pathlen + 1;
                        path = (char *)malloc(n);
                        n = snprintf(path, n, "%s", pd);
                        name = path+pathlen+1;
                        strcpy(name, pn); // This is safe
                       }

             XrdOucPList(const char *pd="", unsigned long long fv=0)
                        : flags(fv), next(0),  path(strdup(pd)),
                          pathlen(strlen(pd)), attrs(0) {}

             XrdOucPList(const char *pd, const char *pn) 
                        : next(0), path(0), attrs(0)
                        {Set(pd, pn);}

            ~XrdOucPList()
                  {if (path) free(path);}

friend class XrdOucPListAnchor;

private:

union{
unsigned long long flags;
char              *name;
     };
XrdOucPList       *next;
char              *path;
int                pathlen;
int                attrs;
};

class XrdOucPListAnchor : public XrdOucPList
{
public:

inline XrdOucPList *About(const char *pathname)
                   {int plen = strlen(pathname); 
                    XrdOucPList *p = next;
                    while(p) {if (p->PathOK(pathname, plen)) break;
                              p=p->next;
                             }
                    return p;
                   }

inline void        Default(unsigned long long x) {dflts = x;}
inline
unsigned long long Default() {return dflts;}
inline void        Defstar(unsigned long long x) {dstrs = x;}

inline void        Empty(XrdOucPList *newlist=0)
                   {XrdOucPList *p = next;
                    while(p) {next = p->next; delete p; p = next;}
                    next = newlist;
                   }

inline unsigned long long  Find(const char *pathname)
                   {int plen = strlen(pathname); 
                    XrdOucPList *p = next;
                    while(p) {if (p->PathOK(pathname, plen)) break;
                              p=p->next;
                             }
                    if (p) return p->flags;
                    return (*pathname == '/' ? dflts : dstrs);
                   }

inline XrdOucPList *Match(const char *pathname)
                   {int plen = strlen(pathname); 
                    XrdOucPList *p = next;
                    while(p) {if (p->pathlen == plen 
                              &&  !strcmp(p->path, pathname)) break;
                              p=p->next;
                             }
                    return p;
                   }

inline XrdOucPList *First() {return next;}

inline void        Insert(XrdOucPList *newitem)
                   {XrdOucPList *pp = 0, *cp = next;
                    while(cp && newitem->pathlen < cp->pathlen) {pp=cp;cp=cp->next;}
                    if (pp) {newitem->next = pp->next; pp->next = newitem;}
                       else {newitem->next = next;         next = newitem;}
                   }

inline int         NotEmpty() {return next != 0;}

                   XrdOucPListAnchor(unsigned long long dfx=0)
                                    : dflts(dfx), dstrs(dfx) {}
                  ~XrdOucPListAnchor() {}

private:

unsigned long long dflts;
unsigned long long dstrs;
};
#endif

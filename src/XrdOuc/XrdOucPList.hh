#ifndef __OUC_PLIST__
#define __OUC_PLIST__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c P L i s t . h h                         */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <strings.h>
#include <stdlib.h>
  
class XrdOucPList
{
public:

inline int          Flag() {return flags;}
inline XrdOucPList *Next() {return next;}
inline char        *Path() {return path;}

inline int          PathOK(const char *pd, const int pl)
                          {return pl >= pathlen && !strncmp(pd, path, pathlen);}

inline void         Set(int fval) {flags = fval;}

             XrdOucPList(char *pathdata=(char *)"", int fvals=0)
                  {next = 0; 
                   pathlen = strlen(pathdata); 
                   path    = strdup(pathdata);
                   flags   = fvals;}

            ~XrdOucPList()
                  {if (path) free(path);}

friend class XrdOucPListAnchor;

private:

XrdOucPList       *next;
int                pathlen;
char              *path;
int                flags;
};

class XrdOucPListAnchor : public XrdOucPList
{
public:

inline void        Default(int x) {dflts = x;}

inline void        Empty(XrdOucPList *newlist=0)
                   {XrdOucPList *p = next;
                    while(p) {next = p->next; delete p; p = next;}
                    next = newlist;
                   }

inline int         Find(const char *pathname)
                   {int plen = strlen(pathname); 
                    XrdOucPList *p = next;
                    while(p) {if (p->PathOK(pathname, plen)) break;
                              p=p->next;
                             }
                    return (p ? p->flags : dflts);
                   }

inline XrdOucPList *First() {return next;}

inline void        Insert(XrdOucPList *newitem)
                   {newitem->next = next; next = newitem;
                   }

inline int         NotEmpty() {return next != 0;}

                   XrdOucPListAnchor() {dflts = 0;}
                  ~XrdOucPListAnchor() {}

private:

int                dflts;
};
#endif

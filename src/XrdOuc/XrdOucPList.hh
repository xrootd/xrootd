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

#include "XrdOuc/XrdOucPthread.hh"
  
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

inline void        Lock() {mutex.Lock();}
inline void      UnLock() {mutex.UnLock();}

inline void        Empty(XrdOucPList *newlist=0)
                   {Lock();
                    XrdOucPList *p = next;
                    while(p) {next = p->next; delete p; p = next;}
                    next = newlist;
                    UnLock();
                   }

inline int         Find(const char *pathname)
                   {int plen = strlen(pathname); 
                    Lock();
                    XrdOucPList *p = next;
                    while(p) {if (p->PathOK(pathname, (const int)plen)) break;
                              p=p->next;
                             }
                    UnLock();
                    return (p ? p->flags : 0);
                   }

inline XrdOucPList *First() {return next;}

inline void        Insert(XrdOucPList *newitem)
                   {Lock();
                    newitem->next = next; next = newitem; 
                    UnLock();
                   }

inline int         NotEmpty() {return next != 0;}

                   // Warning: You must manually lock the object before swap
inline void        Swap(XrdOucPListAnchor &other)
                       {XrdOucPList *savenext = next;
                        next = other.First();
                        other.Zorch(savenext);
                       }

inline void        Zorch(XrdOucPList *newnext=0) {next = newnext;}

private:

XrdOucMutex         mutex;
};
#endif

#ifndef __XrdOucMAPP2X__
#define __XrdOucMAPP2X__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c M a p P 2 X . h h                        */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <cstdlib>

template<class T>
class XrdOucMapP2X
{
public:

XrdOucMapP2X<T>   *Find(const char *path)
                         {XrdOucMapP2X<T> *p = Next;
                          int plen = strlen(path);
                          while(p && plen <= p->PLen)
                               {if (plen ==  p->PLen && !strcmp(p->Path, path))
                                   return p;
                                p = p->Next;
                               }
                          return 0;
                         }

       void        Insert(XrdOucMapP2X<T> *newp)
                         {XrdOucMapP2X<T> *pp = 0, *p = Next;
                          while(p && newp->PLen <  p->PLen)
                               {pp = p; p = p->Next;}
                          newp->Next = p;
                          if (pp) pp->Next = newp;
                             else     Next = newp;
                         }

       bool        isEmpty() {return Next == 0;}

       const char *theName() {return Name;}

XrdOucMapP2X<T>   *theNext() {return Next;}

       const char *thePath() {return Path;}

       T           theValu() {return Valu;}

       void        RepName(const char *newname)
                          {if (Path) {free(Name); Name = strdup(newname);}}

       void        RepValu(T arg) {Valu = arg;}

XrdOucMapP2X<T>      *Match(const char *pd, const int pl=0)
                        {int plen = (pl ? pl : strlen(pd));
                         XrdOucMapP2X<T> *p = Next;
                         while(p && plen >= p->PLen)
                              {if (!strncmp(pd, p->Path, p->PLen)) return p;
                               p=p->Next;
                              }
                         return 0;
                        }

       XrdOucMapP2X() : Next(0), Name(0), Path(0), PLen(0), Valu(0) {}

       XrdOucMapP2X(const char *path, const char *name, T arg=0)
                   : Next(0), Name(strdup(name)), Path(strdup(path)),
                     PLen(strlen(path)), Valu(arg) {}

      ~XrdOucMapP2X() {if (Path) free(Path); if (Name) free(Name);}

private:
       XrdOucMapP2X<T> *Next;
       char            *Name;
       char            *Path;
       int              PLen;
       T                Valu;
};
#endif

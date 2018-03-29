#ifndef __OUC_TLIST__
#define __OUC_TLIST__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c T L i s t . h h                         */
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
  
/******************************************************************************/
/*                     C l a s s   X r d O u c T L i s t                      */
/******************************************************************************/
  
class XrdOucTList
{
public:

XrdOucTList *next;
char        *text;
union
{
long long    dval;
int          ival[2];
short        sval[4];
char         cval[8];
int          val;
};

             XrdOucTList(const char *tval, long long *dv,XrdOucTList *np=0)
                        {next=np; text = (tval ? strdup(tval) : 0); dval=*dv;}

             XrdOucTList(const char *tval=0, int num=0, XrdOucTList *np=0)
                        {next=np; text = (tval ? strdup(tval) : 0); val=num;}

             XrdOucTList(const char *tval, int   iv[2], XrdOucTList *np=0)
                        {next=np; text = (tval ? strdup(tval) : 0);
                         memcpy(ival, iv, sizeof(ival));}

             XrdOucTList(const char *tval, short sv[4], XrdOucTList *np=0)
                        {next=np; text = (tval ? strdup(tval) : 0);
                         memcpy(sval, sv, sizeof(sval));}

             XrdOucTList(const char *tval, char  cv[8], XrdOucTList *np=0)
                        {text = (tval ? strdup(tval) : 0); next=np;
                         memcpy(cval, cv, sizeof(cval));}

            ~XrdOucTList() {if (text) free(text);}
};

/******************************************************************************/
/*               C l a s s   X r d O u c T L i s t H e l p e r                */
/******************************************************************************/
  
class XrdOucTListHelper
{
public:

XrdOucTList **Anchor;

      XrdOucTListHelper(XrdOucTList **p) : Anchor(p) {}
     ~XrdOucTListHelper() {XrdOucTList *tp;
                           while((tp = *Anchor))
                                {*Anchor = tp->next; delete tp;}
                          }
};
  
/******************************************************************************/
/*                 C l a s s   X r d O u c T L i s t F I F O                  */
/******************************************************************************/
  
class XrdOucTListFIFO
{
public:

XrdOucTList *first;
XrdOucTList *last;

inline void  Add(XrdOucTList *tP)
                {if (last) last->next = tP;
                    else   first = tP;
                 last = tP;
                }

inline void  Clear() {XrdOucTList *tP;
                      while((tP = first)) {first = tP->next; delete tP;}
                      first = last = 0;
                     }

XrdOucTList *Pop() {XrdOucTList *tP = first; first = last = 0; return tP;}

             XrdOucTListFIFO() : first(0), last(0) {}
            ~XrdOucTListFIFO() {Clear();}
};
#endif

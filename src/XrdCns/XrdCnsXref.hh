#ifndef __XRDCnsXref_H_
#define __XRDCnsXref_H_
/******************************************************************************/
/*                                                                            */
/*                         X r d C n s X r e f . h h                          */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdCnsXref
{
public:


char           Add(const char *Key, char xref=0);

char          *Apply(int (*func)(const char *, char *, void *), void *Arg)
                    {return xTable.Apply(func, Arg);}

char           Default(const char *Dflt=0);

char          *Key (char  xref);

char           Find(const char *xref);

               XrdCnsXref(const char *Dflt=0, int MTProt=1);
              ~XrdCnsXref();

private:

int              availI();
int              c2i(char xCode);
XrdSysMutex      xMutex;
XrdOucHash<char> xTable;
static char     *xIndex;

static const int yTSize = '~'-'0'+1;
char            *yTable[yTSize];
int              availIdx;
int              isMT;
};
#endif

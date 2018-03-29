#ifndef __XRDPSSCKS_HH__
#define __XRDPSSCKS_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d P s s C k s . h h                           */
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

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksData.hh"

class XrdSysError;

class XrdPssCks : public XrdCks
{
public:

virtual int         Calc( const char *Pfn, XrdCksData &Cks, int doSet=1)
                        {return Get(Pfn, Cks);}

virtual int         Del(  const char *Pfn, XrdCksData &Cks)
                       {return -ENOTSUP;}

virtual int         Get(  const char *Pfn, XrdCksData &Cks);

virtual int         Config(const char *Token, char *Line) {return 1;}

virtual int         Init(const char *ConfigFN, const char *DfltCalc=0);

virtual char       *List(const char *Pfn, char *Buff, int Blen, char Sep=' ')
                        {return 0;}

virtual const char *Name(int seqNum=0);

virtual int         Size( const char  *Name=0);

virtual int         Set(  const char *Pfn, XrdCksData &Cks, int myTime=0)
                       {return -ENOTSUP;}

virtual int         Ver(  const char *Pfn, XrdCksData &Cks);

           XrdPssCks(XrdSysError *erP);
virtual   ~XrdPssCks() {}

private:

struct csInfo
      {char          Name[XrdCksData::NameSize];
       int           Len;
                     csInfo() : Len(0) {memset(Name, 0, sizeof(Name));}
      };

csInfo *Find(const char *Name);

static const int csMax = 4;
csInfo           csTab[csMax];
int              csLast;
};
#endif

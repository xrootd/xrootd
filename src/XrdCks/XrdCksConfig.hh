#ifndef __XRDCKSCONFIG_HH__
#define __XRDCKSCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d C k s C o n f i g . h h                        */
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

#include "XrdOuc/XrdOucTList.hh"

class XrdCks;
class XrdOss;
class XrdOucStream;
class XrdSysError;

struct XrdVersionInfo;
  
class XrdCksConfig
{
public:

XrdCks *Configure(const char *dfltCalc=0, int rdsz=0, XrdOss *ossP=0);

int     Manager() {return CksLib != 0;}

int     Manager(const char *Path, const char *Parms);

const
char   *ManLib() {return CksLib;}

int     ParseLib(XrdOucStream &Config);

        XrdCksConfig(const char *cFN, XrdSysError *Eroute, int &aOK,
                     XrdVersionInfo &vInfo);
       ~XrdCksConfig() {XrdOucTList *tP;
                        if (CksLib)  free(CksLib);
                        if (CksParm) free(CksParm);
                        while((tP = CksList)) {CksList = tP->next; delete tP;}
                       }

private:
XrdCks      *getCks(XrdOss *ossP, int rdsz);

XrdSysError    *eDest;
const char     *cfgFN;
char           *CksLib;
char           *CksParm;
XrdOucTList    *CksList;
XrdOucTList    *CksLast;
XrdVersionInfo &myVersion;
};
#endif

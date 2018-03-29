#ifndef __XRDCNSLogClient_h_
#define __XRDCNSLogClient_h_
/******************************************************************************/
/*                                                                            */
/*                    X r d C n s L o g C l i e n t . h h                     */
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

#include <sys/param.h>
  
#include "XrdSys/XrdSysPthread.hh"

class XrdClient;
class XrdClientAdmin;
class XrdCnsLogFile;
class XrdCnsLogRec;
class XrdCnsXref;
class XrdOucTList;

class XrdCnsLogClient
{
public:

int   Activate(XrdCnsLogFile *basefile);

int   Init();

int   Run(int Always=1);

int   Start();

      XrdCnsLogClient(XrdOucTList *rP, XrdCnsLogClient *pcP);
     ~XrdCnsLogClient() {}

private:
XrdClientAdmin *admConnect(XrdClientAdmin *adminP);

int  Archive(XrdCnsLogFile *lfP);
int  do_Create(XrdCnsLogRec *lrP, const char *lfn=0);
int  do_Mkdir(XrdCnsLogRec *lrP);
int  do_Mv(XrdCnsLogRec *lrP);
int  do_Rm(XrdCnsLogRec *lrP);
int  do_Rmdir(XrdCnsLogRec *lrP);
int  do_Trunc(XrdCnsLogRec *lrP, const char *lfn=0);
char getMount(char *Lfn, char *Pfn, XrdCnsXref &Mount);
int  Inventory(XrdCnsLogFile *lfp, const char *dPath);
int  Manifest();
int  mapError(int rc);
int  xrdEmsg(const char *Opname, const char *theFN, XrdClientAdmin *aP);
int  xrdEmsg(const char *Opname, const char *theFN);
int  xrdEmsg(const char *Opname, const char *theFN, XrdClient *fP);

XrdSysMutex      lfMutex;
XrdSysSemaphore  lfSem;
XrdCnsLogClient *Next;
XrdClientAdmin  *Admin;

XrdCnsLogFile   *logFirst;
XrdCnsLogFile   *logLast;

int              pfxNF;
int              sfxFN;
int              arkOnly;

char            *admURL;
char            *urlHost;

char             arkURL[MAXPATHLEN+512];
char            *arkPath;
char            *arkFN;
char             crtURL[MAXPATHLEN+512];
char            *crtFN;
char             logDir[MAXPATHLEN+1];
char            *logFN;
};
#endif

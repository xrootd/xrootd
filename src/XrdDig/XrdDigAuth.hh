#ifndef __XRDDIGAUTH_HH__
#define __XRDDIGAUTH_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d D i g A u t h . h h                          */
/*                                                                            */
/* (C) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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

#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdOucStream;
class XrdSysError;

/******************************************************************************/
/*                         X r d D i g A u t h E n t                          */
/******************************************************************************/
  
class XrdDigAuthEnt
{
public:
XrdDigAuthEnt *next;
char          *rec;
char           prot[XrdSecPROTOIDSIZE];

enum           eType {eName=0, eHost=1, eVorg=2, eRole=3, eGrp=4, eNum=5};
char          *eChk[eNum];

enum           aType {aConf = 0, aCore = 1, aLogs = 2, aProc = 3, aNum = 4};
bool           accOK[aNum];

               XrdDigAuthEnt() : next(0), rec(0) 
	       {memset(prot, 0, sizeof(prot));
		memset(eChk, 0, sizeof(eChk));
		memset(accOK, 0, sizeof(accOK));
	       }
              ~XrdDigAuthEnt() {if (rec) free(rec);}
};

/******************************************************************************/
/*                            X r d D i g A u t h                             */
/******************************************************************************/
  
class XrdDigAuth
{
public:

bool  Authorize(const XrdSecEntity   *client,
                XrdDigAuthEnt::aType  aType,
                bool                  aVec[XrdDigAuthEnt::aNum]=0
               );

bool  Configure(const char *aFN);

      XrdDigAuth() : authFN(0), authTOD(0), authCHK(0), authList(0) {}
     ~XrdDigAuth() {}

private:

bool Failure(int lNum, const char *txt1, const char *txt2=0);
bool OkGrp(const char *glist, const char *gname);
bool Parse(XrdOucStream &aFile, int lNum);
bool Refresh();
bool SetupAuth(bool isRefresh);
bool SetupAuth(bool isRefresh, bool aOK);
void Squash(char *bP);

XrdSysMutex    authMutex;
const char    *authFN;
time_t         authTOD;
time_t         authCHK;
XrdDigAuthEnt *authList;
bool           accOK[XrdDigAuthEnt::aNum];
};
#endif

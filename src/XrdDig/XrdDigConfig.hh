#ifndef __XRDDIGCONFIG_HH__
#define __XRDDIGCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d D i g C o n f i g . h h                        */
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

class XrdOucStream;
class XrdOucErrInfo;
class XrdSecEntity;
struct stat;

class XrdDigConfig
{
public:


bool  Configure(const char *cFN, const char *parms);

enum  pType {isAny = 0, isDir, isFile};

int   GenAccess(const XrdSecEntity *client,
                const char         *aList[],
                int                 aMax);

char *GenPath(int &rc, const XrdSecEntity *client, const char *opname,
                       const char         *lfn,    pType lfnType=isAny);

void  GetLocResp(XrdOucErrInfo &eInfo, bool nameok);

static
void  StatRoot(struct stat *sP);

      XrdDigConfig() : fnTmplt(0), logAcc(true), logRej(true) {}
     ~XrdDigConfig()   {}

private:

const char *AddPath(XrdDigConfig::pType sType, const char *src,
                              const char *tpd, const char *tfn);
void        Audit(const XrdSecEntity *client, const char *what,
                  const char         *opn,    const char *trg);
bool        ConfigProc(const char *ConfigFN);
bool        ConfigXeq(char *var, XrdOucStream &cFile);
void        Empty(const char *path);
void        SetLocResp();
int         ValProc(const char *ppath);
bool        xacf(XrdOucStream &cFile);
bool        xlog(XrdOucStream &cFile);

char       *fnTmplt;
char       *locRespHP;
char       *locRespV6;
char       *locRespV4;
short       locRlenHP;
short       locRlenV6;
short       locRlenV4;
bool        logAcc;
bool        logRej;
};
#endif

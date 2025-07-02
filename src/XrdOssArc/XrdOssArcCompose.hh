#ifndef _XRDOSSARCCOMPOSE_H
#define _XRDOSSARCCOMPOSE_H
/******************************************************************************/
/*                                                                            */
/*                   X r d O s s A r c C o m p o s e . h h                    */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <string>

struct stat;
class  XrdOucEnv;

class XrdOssArcCompose
{

public:

std::string dsScope; // Dataset scope
std::string dsName;  // Dataset name
std::string flScope; // File    scope
std::string flName;  // File    name
std::string arName;  // Archive filename

enum pType {isARC=0, isBKP};
     pType didType;

       int         ArcMember(char* buff, int blen);

       int         ArcPath(char* buff, int blen, bool addafn=false);

static std::string Dir2DSN(const char* dir);
static std::string DSN2Dir(const char* dsn);

static bool        isArcFile(const char *path);

static bool        isArcPath(const char* path);

static bool        isBkpPath(const char* path);

static bool        isMine(const char *path);

static int         Stat(const char* Scope, const char* Name, struct stat* Stat);

       XrdOssArcCompose(const char *path, XrdOucEnv *env,
                        int& retc, bool isW=true, bool optfn=false);

      ~XrdOssArcCompose() {}

private:

       int   getDSN(const char* path);
       int   SetarName();
static int   StatDecode(struct stat& Stat, const char* resp);
static bool  StatGet(const char* var, XrdOucEnv& env, long long& val);

static int   minLenDSN;
static int   minLenFN;
};
#endif

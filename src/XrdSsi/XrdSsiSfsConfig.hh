#ifndef __SSISFS_CONFIG_HH__
#define __SSISFS_CONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d S s i S f s C o n f i g . h h                     */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdOucEnv;
class XrdOucStream;
class XrdSsiCluster;
class XrdSsiServer;
class XrdVersionInfo;

class XrdSsiSfsConfig
{
public:

XrdVersionInfo  *myVersion;
const char      *myHost;
const char      *myProg;
const char      *myInsName;
char            *myRole;
XrdSsiCluster   *SsiCms;
int              myPort;
bool             isServer;
bool             isCms;

bool             Configure(const char *cFN, XrdOucEnv *envP);

bool             Configure(XrdOucEnv *envP);

                 XrdSsiSfsConfig(bool iscms=false);

                ~XrdSsiSfsConfig();

private:

XrdOucStream *cFile;
char         *ConfigFN;       //    ->Configuration filename
char         *CmsLib;         //    ->Cms Library
char         *CmsParms;       //    ->Cms Library Parameters
char         *SvcLib;         //    ->Svc Library
char         *SvcParms;       //    ->Svc Library Parameters
int           roleID;

int           ConfigCms(XrdOucEnv *envP);
int           ConfigObj();
int           ConfigSvc(char **myArgv, int myArgc);
int           ConfigXeq(char *var);
int           Xlib(const char *lName, char **lPath, char **lParm);
int           Xfsp();
int           Xopts();
int           Xrole();
int           Xtrace();
};
#endif

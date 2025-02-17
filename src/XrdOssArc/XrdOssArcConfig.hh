#ifndef __XRDOSSARCCONFIG_HH__
#define __XRDOSSARCCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O s s A r c C o n f i g . h h                     */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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
class XrdOucProg;
class XrdOucTokenizer;

class XrdOssArcConfig
{
public:

bool  Configure(const char* cfn, const char* parms, XrdOucEnv* envP);

int   GenLocalPath(const char* dsn, char* buff, int bSZ);

int   GenTapePath(const char* dsn, char* buff, int bSZ, bool addafn=false);
  
      XrdOssArcConfig();
     ~XrdOssArcConfig() {} // Never gets deleted

// Program section (i.e. the various scripts to launch
//
XrdOucProg* getManProg;    // Obtain the list of files in a dataset
char*       getManPath;    // The actual path to the program
const char* getManEOL;     // Character sequence to indicate EOL

XrdOucProg* ArchiverProg;  // Create an archive
char*       ArchiverPath;  // The actual path to the executable

XrdOucProg* MssComProg;    // Communicate with the MSS
char*       MssComPath;    // Path to prog script
char*       MssComCmd;     // Actual command to be invoked
char*       MssComRoot;    // Root of the MSS Tape File System

// Path section
//
char*       arcvPathLFN;   // LFN of archive path, default "/archive/"
int         arcvPathLEN;   // Length of the above
int         bkupPathLEN;   // Length of the below
char*       bkupPathLFN;   // LFN of backup path, default "/backup/"
char*       stagePath;     // Path to directory where zip members are extracted
char*       tapePath;      // The full path of the tape disk buffer
char*       utilsPath;     // Default path to utils

// Miscellaneous
//
int         maxStage;      // Maximum number of parallel stages
int         wtpStage;      // Staging Wait/Poll interval 
char*       arFName;       // Full archive filename (e.g. archive.zip)
char*       arfSfx;        // Archive file suffix
int         arfSfxLen;     // Length of the above
char        mySep;         // Slash replacement separator

private:

void ConfigPath(char** pDest, const char* pRoot);
bool ConfigProc(const char* drctv, const char *lastLine);
bool ConfigXeq(const char* cfName, const char* parms, XrdOucEnv* envP);
int  MissArg(const char* what);
bool Usable(const char* path, const char* what, bool useOss=true);
int  xqGrab(const char* what, char*& theDest, const char* theLine);
int  xqPaths();
int  xqStage();
int  xqTrace();
int  xqUtils();

XrdOucTokenizer* Conf;
};
#endif

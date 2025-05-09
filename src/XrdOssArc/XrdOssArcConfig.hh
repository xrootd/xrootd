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
class XrdOucGatherConf;

class XrdOssArcConfig
{
public:

bool  BuildPath(const char* what, const char* baseP, 
                const char* addP, char*& destP, int mode=0);

bool  Configure(const char* cfn, const char* parms, XrdOucEnv* envP);

int   GenArcPath(const char* dsn, char* buff, int bSZ);

int   GenLocalPath(const char* dsn, char* buff, int bSZ);

int   GenTapePath(const char* dsn, char* buff, int bSZ, bool addafn=false);
  
      XrdOssArcConfig();
     ~XrdOssArcConfig() {} // Never gets deleted

// Program section (i.e. the various scripts to launch
//
XrdOucProg* BkpUtilProg;   // Various Rucio involved functions
char*       BkpUtilPath;   // The actual path to the program
const char* BkpUtilEOL;    // Character sequence to indicate EOL

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
char*       bkupPathLFN;   // LFN of backup  path, default "/backup/"
char*       dsetPathLFN;   // LFN of dataset path, default "/dataset/"
char*       dsetRepoPFN;   // Path to directory where dataset backups are staged
char*       srcData;       // Root path to where srcrse data is mounted
char*       stagePath;     // Path to directory where zip members are extracted
char*       tapePath;      // The full path of the tape disk buffer
char*       utilsPath;     // Default path to utils

// Miscellaneous
//
const char* metaBKP;       // Metadat variable name
const char* metaIDX;       // Metadat variable name
char*       doneBKP;       // Metadata value indicating a backup completed
char*       needBKP;       // Metadata value indicating a backup is needed
char*       dstRSE;        // The name of the dest rse (our name)
char*       srcRSE;        // The name of the source rse
long long   bkpMinF;       // Percentage or bytes that must be always available
int         bkpMax;        // Maximum number of parallel backups
int         bkpPoll;       // Polling interval to find new items to backup
int         bkpFSt;        // Backup fs scan interval in seconds
int         maxStage;      // Maximum number of parallel stages
int         wtpStage;      // Staging Wait/Poll interval 
int         r_maxItems;    // rucio maximum response lines (query limit)
char*       arFName;       // Full archive filename (e.g. archive.zip)
char*       arfSfx;        // Archive file suffix
int         arfSfxLen;     // Length of the above
char        mySep;         // Slash replacement separator

bool        arcSZ_Skip;    // When true skip archiving if size can't be met
long long   arcSZ_Want;    // Preferred size of archive
long long   arcSZ_MinV;    // Minimum size archive can have
long long   arcSZ_MaxV;    // Maximum size archive can have

private:
void ConfigPath(char** pDest, const char* pRoot);
bool ConfigProc(const char* drctv);
bool ConfigXeq(const char* cfName, const char* parms, XrdOucEnv* envP);
bool MissArg(const char* what);
bool Usable(const char* path, const char* what, bool useOss=true);
bool xqGrab(const char* what, char*& theDest, const char* theLine);
bool xqArcsz();
bool xqBkup();
bool xqBkupPS(char* tval);
bool xqBkupScope();
bool xqPaths();
bool xqRse();
bool xqRucio();
bool xqStage();
bool xqTrace();
bool xqUtils();

XrdOucGatherConf* Conf;
};
#endif

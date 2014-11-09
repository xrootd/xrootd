#ifndef __XRDCPFILE_HH__
#define __XRDCPFILE_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d C p F i l e . h h                           */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <stdlib.h>
#include <string.h>

class XrdCpFile
{
public:

enum PType {isOther = 0, isDir,  isFile,  isStdIO,
            isXroot,     isHttp, isHttps, isDevNull, isDevZero
           };

XrdCpFile        *Next;         // -> Next file in list
char             *Path;         // -> Absolute path to the file
short             Doff;         //    Offset to directory extension in Path
short             Dlen;         //    Length of directory extension (0 if none)
                                //    The length includes the trailing slash.
PType             Protocol;     //    Protocol type
char              ProtName[8];  //    Protocol name
long long         fSize;        //    Size of file

int               Extend(XrdCpFile **pLast, int &nFile, long long &nBytes);

int               Resolve();

static void       SetMsgPfx(const char *pfx) {mPfx = pfx;}

                  XrdCpFile() : Next(0), Path(0), Doff(0), Dlen(0),
                                Protocol(isOther), fSize(0) {*ProtName = 0;}

                  XrdCpFile(const char *FSpec, int &badURL);

                  XrdCpFile(      char *FSpec, struct stat &Stat,
                                  short doff,         short dlen);

                 ~XrdCpFile() {if (Path) free(Path);}
private:

static const char *mPfx;
};
#endif

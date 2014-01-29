#ifndef __XRDPOSIXDIR_H__
#define __XRDPOSIXDIR_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x D i r . h h                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/

#include <dirent.h>

#if  defined(__APPLE__)
#if !defined(dirent64)
#define dirent64 dirent
#endif
#endif

#include <unistd.h>
#include <sys/types.h>

#include "XrdPosix/XrdPosixAdmin.hh"
#include "XrdPosix/XrdPosixObject.hh"

class XrdPosixDir : public XrdPosixObject
{
public:
                   XrdPosixDir(const char *path)
                              : DAdmin(path), myDirVec(0), myDirEnt(0),
                                nxtEnt(0), numEnt(0), eNum(0)
                              {}

                  ~XrdPosixDir() {delete myDirVec;
                                  if (myDirEnt) free(myDirEnt);
                                 }

static int         dirNo(DIR *dirP)  {return *(int *)dirP;}

       long        getEntries() { return numEnt;}

       long        getOffset() { return nxtEnt; }

       void        setOffset(long offset) { nxtEnt = offset; }

       dirent64   *nextEntry(dirent64 *dp=0);

       DIR        *Open();

       void        rewind() {Lock();
                             nxtEnt = 0; delete myDirVec; myDirVec = 0;
                             UnLock();
                            }
       int         Status() {return eNum;}

       bool        Unread() {return myDirVec == 0;}

       using       XrdPosixObject::Who;

       bool        Who(XrdPosixDir **dirP) {*dirP = this; return true;}

static const size_t maxDlen = 256;

private:
  XrdPosixAdmin         DAdmin;
  XrdCl::DirectoryList *myDirVec;
  dirent64             *myDirEnt;
  uint32_t              nxtEnt;
  uint32_t              numEnt;
  int                   eNum;
};
#endif

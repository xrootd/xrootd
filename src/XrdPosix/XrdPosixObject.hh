#ifndef __XRDPOSIXOBJECT_HH__
#define __XRDPOSIXOBJECT_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x O b j e c t . h h                      */
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
/******************************************************************************/

#include <sys/types.h>

#include "XrdSys/XrdSysPthread.hh"

class XrdPosixDir;
class XrdPosixFile;

class XrdPosixObject
{
public:

        bool          AssignFD(bool isStream=false);

static  bool          CanStream() {return baseFD == 0 && freeFD < 255;}

static  XrdPosixDir  *Dir (int fildes, bool glk=false);

static  XrdPosixFile *File(int fildes, bool glk=false);

        int           FDNum() {return fdNum;}

static  int           Init(int numfd);

static  void          Release(XrdPosixObject *oP, bool needlk=true);

static  XrdPosixDir  *ReleaseDir( int fildes);

static  XrdPosixFile *ReleaseFile(int fildes);

static  void          Shutdown();

        void          UnLock() {objMutex.UnLock();}

static  bool          Valid(int fd)
                           {return fd >= baseFD && fd <= (highFD+baseFD)
                                   && myFiles && myFiles[fd-baseFD];}

virtual bool          Who(XrdPosixDir  **dirP)  {return false;}

virtual bool          Who(XrdPosixFile **fileP) {return false;}

                      XrdPosixObject() : fdNum(-1) {}
virtual              ~XrdPosixObject() {if (fdNum >= 0) Release(this);}

protected:
       XrdSysRWLock     objMutex;
       int              fdNum;

private:

static XrdSysMutex      fdMutex;
static XrdPosixObject **myFiles;
static int              lastFD;
static int              highFD;
static int              baseFD;
static int              freeFD;
static int              devNull;
};
#endif

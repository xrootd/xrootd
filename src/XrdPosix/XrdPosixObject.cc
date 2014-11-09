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

#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include "XrdPosix/XrdPosixObject.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysMutex      XrdPosixObject::fdMutex;
XrdPosixObject **XrdPosixObject::myFiles  =  0;
int              XrdPosixObject::highFD   = -1;
int              XrdPosixObject::lastFD   = -1;
int              XrdPosixObject::baseFD   =  0;
int              XrdPosixObject::freeFD   =  0;
int              XrdPosixObject::devNull  = -1;

/******************************************************************************/
/*                              A s s i g n F D                               */
/******************************************************************************/
  
bool XrdPosixObject::AssignFD(bool isStream)
{
   XrdSysMutexHelper fdHelper(fdMutex);
   int  fd;

// Obtain a new filedscriptor from the system. Use the fd to track the object.
// Streams are not supported for virtual file descriptors.
//
   if (baseFD)
      { if (isStream) return 0;
        for (fd = freeFD; fd < baseFD && myFiles[fd]; fd++) {}
        if (fd >= baseFD) return 0;
        freeFD = fd+1;
      } else {
        do{if ((fd = dup(devNull)) < 0) return false;
           if (fd >= lastFD || (isStream && fd > 255))
              {close(fd); return 0;}
           if (!myFiles[fd]) break;
           cerr <<"XrdPosix: FD " <<fd <<" closed outside of XrdPosix!" <<endl;
          } while(1);
      }

// Enter object in out vector of objects and assign it the FD
//
   myFiles[fd] = this;
   if (fd > highFD) highFD = fd;
   fdNum  = fd + baseFD;
   fdMutex.UnLock();

// All done.
//
   return true;
}
  
/******************************************************************************/
/*                                   D i r                                    */
/******************************************************************************/
  
XrdPosixDir *XrdPosixObject::Dir(int fd, bool glk)
{
   XrdPosixDir    *dP;
   XrdPosixObject *oP;
   int  waitCount = 0;
   bool haveLock;

// Validate the fildes
//
do{if (fd >= lastFD || fd < baseFD)
      {errno = EBADF; return (XrdPosixDir *)0;}

// Obtain the file object, if any
//
   fdMutex.Lock();
   if (!(oP = myFiles[fd - baseFD]) || !(oP->Who(&dP)))
      {fdMutex.UnLock(); errno = EBADF; return (XrdPosixDir *)0;}

// Attempt to lock the object in the appropriate mode. If we fail, then we need
// to retry this after dropping the global lock. We pause a bit to let the
// current lock holder a chance to unlock the lock. We only do this a limited
// amount of time (1 minute) so that we don't get stuck here forever.
//
   if (glk) haveLock = oP->objMutex.CondWriteLock();
      else  haveLock = oP->objMutex.CondReadLock();
   if (!haveLock)
      {fdMutex.UnLock();
       waitCount++;
       if (waitCount > 120) break;
       XrdSysTimer::Wait(500); // We wait 500 milliseconds
       continue;
      }

// If the global lock is to be held, then release the object lock as this
// is a call to destroy the object and there is no need for the local lock.
//
   if (glk) oP->UnLock();
      else  fdMutex.UnLock();
   return dP;
  } while(1);

// If we get here then we timedout waiting for the object lock
//
   errno = ETIMEDOUT;
   return (XrdPosixDir *)0;
}
  
/******************************************************************************/
/*                                  F i l e                                   */
/******************************************************************************/
  
XrdPosixFile *XrdPosixObject::File(int fd, bool glk)
{
   XrdPosixFile   *fP;
   XrdPosixObject *oP;
   int  waitCount = 0;
   bool haveLock;

// Validate the fildes
//
do{if (fd >= lastFD || fd < baseFD)
      {errno = EBADF; return (XrdPosixFile *)0;}

// Obtain the file object, if any
//
   fdMutex.Lock();
   if (!(oP = myFiles[fd - baseFD]) || !(oP->Who(&fP)))
      {fdMutex.UnLock(); errno = EBADF; return (XrdPosixFile *)0;}

// Attempt to lock the object in the appropriate mode. If we fail, then we need
// to retry this after dropping the global lock. We pause a bit to let the
// current lock holder a chance to unlock the lock. We only do this a limited
// amount of time (1 minute) so that we don't get stuck here forever.
//
   if (glk) haveLock = oP->objMutex.CondWriteLock();
      else  haveLock = oP->objMutex.CondReadLock();
   if (!haveLock)
      {fdMutex.UnLock();
       waitCount++;
       if (waitCount > 120) break;
       XrdSysTimer::Wait(500); // We wait 500 milliseconds
       continue;
      }

// If the global lock is to be held, then release the object lock as this
// is a call to destroy the object and there is no need for the local lock.
//
   if (glk) oP->UnLock();
      else  fdMutex.UnLock();
   return fP;
  } while(1);

// If we get here then we timedout waiting for the object lock
//
   errno = ETIMEDOUT;
   return (XrdPosixFile *)0;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdPosixObject::Init(int fdnum)
{
   struct rlimit rlim;
   int isize;

// Initialize the /dev/null file descriptors, bail if we cannot
//
   devNull = open("/dev/null", O_RDWR, 0744);
   if (devNull < 0) return -1;

// Compute size of table. if the passed fdnum is negative then the caller does
// not want us to shadow fd's (ther caller promises to be honest). Otherwise,
// the actual fdnum limit will be based on the current limit.
//
   if (fdnum < 0)
      {fdnum = -fdnum;
       baseFD = ( getrlimit(RLIMIT_NOFILE, &rlim) ? 32768 : (int)rlim.rlim_cur);
      } else {
       if (!getrlimit(RLIMIT_NOFILE, &rlim))  fdnum = (int)rlim.rlim_cur;
       if (fdnum > 65536) fdnum = 65536;
      }
   isize = fdnum * sizeof(XrdPosixFile *);

// Allocate the table for fd-type pointers
//
   if (!(myFiles = (XrdPosixObject **)malloc(isize))) lastFD = -1;
      else {memset((void *)myFiles, 0, isize); lastFD = fdnum+baseFD;}

// All done
//
   return baseFD;
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
void XrdPosixObject::Release(XrdPosixObject *oP, bool needlk)
{
// Get the lock if need be
//
   if (needlk) fdMutex.Lock();

// Remove the object from the table
//
   if (baseFD)
      {int myFD = oP->fdNum - baseFD;
       if (myFD < freeFD) freeFD = myFD;
       myFiles[myFD] = 0;
      } else {
       myFiles[oP->fdNum] = 0;
       close(oP->fdNum);
      }

// Zorch the object fd and relese the global lock (object lock still held)
//
   oP->fdNum = -1;
   fdMutex.UnLock();
}

/******************************************************************************/
/*                            R e l e a s e D i r                             */
/******************************************************************************/
  
XrdPosixDir *XrdPosixObject::ReleaseDir(int fd)
{
   XrdPosixDir    *dP;
  
// Find the directory object
//
   if (!(dP = Dir(fd, true))) return (XrdPosixDir *)0;

// Release it and return the underlying object
//
   Release((XrdPosixObject *)dP, false);
   return dP;
}

/******************************************************************************/
/*                           R e l e a s e F i l e                            */
/******************************************************************************/
  
XrdPosixFile *XrdPosixObject::ReleaseFile(int fd)
{
   XrdPosixFile   *fP;
  
// Find the file object
//
   if (!(fP = File(fd, true))) return (XrdPosixFile *)0;

// Release it and return the underlying object
//
   Release((XrdPosixObject *)fP, false);
   return fP;
}
  
/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/
  
void XrdPosixObject::Shutdown()
{
   XrdPosixObject *oP;
   int i;

// Destory all files and static data
//
   fdMutex.Lock();
   if (myFiles)
      {for (i = 0; i <= highFD; i++) 
           if ((oP = myFiles[i]))
              {myFiles[i] = 0;
               if (oP->fdNum >= 0) close(oP->fdNum);
               oP->fdNum = -1;
               delete oP;
              };
       free(myFiles); myFiles = 0;
      }
   fdMutex.UnLock();
}

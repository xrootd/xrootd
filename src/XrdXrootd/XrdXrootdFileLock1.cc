/******************************************************************************/
/*                                                                            */
/*                 X r d X r o o t d F i l e L o c k 1 . c c                  */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucHash.hh"

#include "XrdXrootd/XrdXrootdFileLock1.hh"
 
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdXrootdFileLockInfo
{
public:

int   numReaders;
int   numWriters;

      XrdXrootdFileLockInfo(char mode)
                      {if (mode == 'r') {numReaders = 1; numWriters = 0;}
                          else          {numReaders = 0; numWriters = 1;}
                      }
     ~XrdXrootdFileLockInfo() {}
};

class XrdXrootdLockFileLock
{
public:

      XrdXrootdLockFileLock(XrdSysMutex *mutex)
                      {mp = mutex; mp->Lock();}
     ~XrdXrootdLockFileLock()
                      {mp->UnLock();}
private:
XrdSysMutex *mp;
};

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdOucHash<XrdXrootdFileLockInfo> XrdXrootdLockTable;

XrdSysMutex  XrdXrootdFileLock1::LTMutex;

const char *XrdXrootdFileLock1::TraceID = "FileLock1";
 
/******************************************************************************/
/*                                  L o c k                                   */
/******************************************************************************/
  
int XrdXrootdFileLock1::Lock(const char *path, char mode, bool force)
{
   XrdXrootdLockFileLock locker(&LTMutex);
   XrdXrootdFileLockInfo *lp;

// See if we already have a lock on this file
//
   if ((lp = XrdXrootdLockTable.Find(path)))
      {if (mode == 'r')
          {if (lp->numWriters && !force)
              return -lp->numWriters;
           lp->numReaders++;
          } else {
           if ((lp->numReaders || lp->numWriters) && !force)
              return (lp->numWriters ? -lp->numWriters : lp->numReaders);
           lp->numWriters++;
          }
       return 0;
      }

// Item does not exist, add it to the table
//
   XrdXrootdLockTable.Add(path, new XrdXrootdFileLockInfo(mode));
   return 0;
}
 
/******************************************************************************/
/*                                                                            */
/*                              n u m L o c k s                               */
/*                                                                            */
/******************************************************************************/

void XrdXrootdFileLock1::numLocks(const char *path, int &rcnt, int &wcnt)
{
   XrdXrootdLockFileLock locker(&LTMutex);
   XrdXrootdFileLockInfo *lp;

   if (!(lp = XrdXrootdLockTable.Find(path))) rcnt = wcnt = 0;
      else {rcnt = lp->numReaders; wcnt = lp->numWriters;}
}
  
/******************************************************************************/
/*                                U n l o c k                                 */
/******************************************************************************/
  
int XrdXrootdFileLock1::Unlock(const char *path, char mode)
{
   XrdXrootdLockFileLock locker(&LTMutex);
   XrdXrootdFileLockInfo *lp;

// See if we already have a lock on this file
//
   if (!(lp = XrdXrootdLockTable.Find(path))) return 1;

// Adjust the lock information
//
   if (mode == 'r')
      {if (lp->numReaders == 0) return 1;
       lp->numReaders--;
      } else {
       if (lp->numWriters == 0) return 1;
       lp->numWriters--;
      }

// Delete the entry if we no longer need it
//
   if (lp->numReaders == 0 && lp->numWriters == 0)
      XrdXrootdLockTable.Del(path);
   return 0;
}

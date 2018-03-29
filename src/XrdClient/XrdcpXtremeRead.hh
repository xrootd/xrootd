#ifndef XRDCPXTREMEREAD_HH
#define XRDCPXTREMEREAD_HH
/******************************************************************************/
/*                                                                            */
/*                  X r d c p X t r e m e R e a d . h h                       */
/*                                                                            */
/* Author: Fabrizio Furano (CERN, 2009)                                       */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Utility classes handling Extreme readers, i.e. coordinated parallel  //
//  reads from multiple XrdClient instances                             //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdSys/XrdSysPthread.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientVector.hh"

class XrdXtRdBlkInfo {
public:
   long long offs;
   int len;
   time_t lastrequested;

   // Nothing more to do, block acquired
   bool done;

   // The seq of the clientidxs which requested this blk
   XrdClientVector<int> requests;

   bool AlreadyRequested(int clientIdx) {
      for (int i = 0; i < requests.GetSize(); i++)
         if (requests[i] == clientIdx) return true;
      return false;
   }

   XrdXtRdBlkInfo() {offs = 0; len = 0; done = false; requests.Clear(); lastrequested = 0; }
};

class XrdXtRdFile {
private:
   int clientidxcnt;         // counter to assign client idxs
   XrdSysRecMutex mtx;       // mutex to protect data structures

   int freeblks;    // Blocks not yet assigned to readers
   int nblks;       // Total number of blocks
   int doneblks;    // Xferred blocks

   XrdXtRdBlkInfo *blocks;

public:

   // Models a file as a sequence of blocks, which can be attrbuted to
   //  different readers
   XrdXtRdFile(int blksize, long long filesize);
   ~XrdXtRdFile();

   bool AllDone() { XrdSysMutexHelper m(mtx); return (doneblks >= nblks); }

   // Gives a unique ID which can identify a reader client in the game
   int GimmeANewClientIdx();

   int GetNBlks() { return nblks; }

   // Finds a block to prefetch and then read
   // Atomically associates it to a client idx
   // Returns the blk index
   int GetBlkToPrefetch(int fromidx, int clientIdx, XrdXtRdBlkInfo *&blkreadonly);
   int GetBlkToRead(int fromidx, int clientidx, XrdXtRdBlkInfo *&blkreadonly);

   void MarkBlkAsRequested(int blkidx);
   int MarkBlkAsRead(int blkidx);

   static int GetListOfSources(XrdClient *ref, XrdOucString xtrememgr,
                               XrdClientVector<XrdClient *> &clients,
                               int maxSources=12);


};
#endif

/******************************************************************************/
/*                                                                            */
/*                  X r d c p X t r e m e R e a d . c c                       */
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
// Utility classes handling coordinated parallel reads from multiple    //
// XrdClient instances                                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdcpXtremeRead.hh"
#include "XrdClient/XrdClientAdmin.hh"

XrdXtRdFile::XrdXtRdFile(int blksize, long long filesize) {
   blocks = 0;
   clientidxcnt = 0;
   freeblks = 0;
   doneblks = 0;

   freeblks = nblks = (filesize + blksize - 1) / blksize;

   blocks = new XrdXtRdBlkInfo[nblks];

   // Init the list of blocks
   long long ofs = 0;
   for (int i = 0; i < nblks; i++) {
      blocks[i].offs = ofs;
      blocks[i].len = xrdmax(0, xrdmin(filesize, ofs+blksize) - ofs);
      ofs += blocks[i].len;
   }

}

XrdXtRdFile::~XrdXtRdFile() {
   delete []blocks;
}

int XrdXtRdFile::GimmeANewClientIdx() {
   XrdSysMutexHelper m(mtx);
   return ++clientidxcnt;
}

int XrdXtRdFile::GetBlkToPrefetch(int fromidx, int clientidx, XrdXtRdBlkInfo *&blkreadonly) {
   // Considering fromidx as a starting point in the blocks array,
   // finds a block which is worth prefetching
   // If there are free blocks it's trivial
   // Otherwise it will be stolen from other readers which are clearly late

   XrdSysMutexHelper m(mtx);


   // Find a non assigned blk
   for (int i = 0; i < nblks; i++) {
      int pos = (fromidx + i) % nblks;

      // Find a non assigned blk
      if (blocks[pos].requests.GetSize() == 0) {
         blocks[pos].requests.Push_back(clientidx);
         blocks[pos].lastrequested = time(0);
         blkreadonly = &blocks[pos];
         return pos;
      }   
   }

   // Steal an outstanding missing block, even if in progress
   // The outcome of this is that, at the end, all thethe fastest free clients will
   // ask for the missing blks
   // The only thing to avoid is that a client asks twice the same blk for itself

   for (int i = nblks; i > 0; i--) {
      int pos = (fromidx + i) % nblks;

      // Find a non finished blk to steal
      if (!blocks[pos].done && !blocks[pos].AlreadyRequested(clientidx) &&
          (blocks[pos].requests.GetSize() < 3) ) {

         blocks[pos].requests.Push_back(clientidx);
         blkreadonly = &blocks[pos];
         blocks[pos].lastrequested = time(0);
         return pos;
      }
   }

   // No blocks to request or steal... probably everything's finished
   return -1;

}

int XrdXtRdFile::GetBlkToRead(int fromidx, int clientidx, XrdXtRdBlkInfo *&blkreadonly) {
   // Get the next already prefetched block, now we want to get its content

   XrdSysMutexHelper m(mtx);

   for (int i = 0; i < nblks; i++) {
      int pos = (fromidx + i) % nblks;
      if (!blocks[pos].done &&
          blocks[pos].AlreadyRequested(clientidx)) {

         blocks[pos].lastrequested = time(0);
         blkreadonly = &blocks[pos];
         return pos;
      }
   }

   return -1;
}

int XrdXtRdFile::MarkBlkAsRead(int blkidx) {
   XrdSysMutexHelper m(mtx);

   int reward = 0;

   // If the block was stolen by somebody else then the reward is negative
   if (blocks[blkidx].done) reward = -1;
   if (!blocks[blkidx].done) {
      doneblks++;
      if (blocks[blkidx].requests.GetSize() > 1) reward = 1;
   }


   blocks[blkidx].done = true;
   return reward;
}


int XrdXtRdFile::GetListOfSources(XrdClient *ref, XrdOucString xtrememgr,
                                  XrdClientVector<XrdClient *> &clients,
                                  int maxSources)
{
   // Exploit Locate in order to find as many sources as possible.
   // Make sure that ref appears once and only once
   // Instantiate and open the relative client instances

   XrdClientVector<XrdClientLocate_Info> hosts;
   if (xtrememgr == "") return 0;

   // In the simple case the xtrememgr is just the host of the original url.
   if (!xtrememgr.beginswith("root://") && !xtrememgr.beginswith("xroot://")) {
      
      // Create an acceptable xrootd url
      XrdOucString loc2;
      loc2 = "root://";
      loc2 += xtrememgr;
      loc2 += "/xyz";
      xtrememgr = loc2;
   }

   XrdClientAdmin adm(xtrememgr.c_str());
   if (!adm.Connect()) return 0;

   int locateok = adm.Locate((kXR_char *)ref->GetCurrentUrl().File.c_str(), hosts, kXR_nowait);
   if (!locateok || !hosts.GetSize()) return 0;
   if (maxSources > hosts.GetSize()) maxSources = hosts.GetSize();

   // Here we have at least a result... hopefully
   bool found = false;
   for (int i = 0; i < maxSources; i++)
      if (ref->GetCurrentUrl().HostWPort == (const char *)(hosts[i].Location)) {
         found = true;
         break;
      }

   // Now initialize the clients and start the parallel opens
   for (int i = 0; i < maxSources; i++) {
      XrdOucString loc;

      loc = "root://";
      loc += (const char *)hosts[i].Location;
      loc += "/";
      loc += ref->GetCurrentUrl().File;
      cout << "Source #" << i+1 << " " << loc << endl;

      XrdClient *cli = new XrdClient(loc.c_str());
      if (cli) {
            clients.Push_back(cli);

      }

   }

   // Eventually add the ref client to the vector
   if (!found && ref) clients.Push_back(ref);

   return clients.GetSize();
}

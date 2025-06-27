/******************************************************************************/
/*                                                                            */
/*                      X r d N e t R e f r e s h . c c                       */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <string>

#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetPeer.hh"
#include "XrdNet/XrdNetRefresh.hh"
#include "XrdNet/XrdNetUtils.hh"

#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdNetSocketCFG
{
XrdNetRefresh* NetRefresh; // This may not be deleted once allocated

//int            udpRefr = 8*60*60;
int            udpRefr = 30;
};
using namespace XrdNetSocketCFG;

/******************************************************************************/
/*                  L o c a l   S t a t i c   O b j e c t s                   */
/******************************************************************************/

namespace
{
XrdScheduler* schedP;
XrdSysError   eDest(0, "XrdNet");

struct RefInfo
      {std::string    destHN;
       unsigned int   Instance;

                      RefInfo(const char* hname, unsigned int inum)
                             : destHN(hname), Instance(inum) {}
                     ~RefInfo() {}

      };

std::map<int, RefInfo> fd2Info;

XrdSysMutex refMTX;
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

void XrdNetRefresh::DoIt()
{

// Run the address updater
//
   Update();

// Reschedule ourselves
//
   schedP->Schedule(this, time(0)+udpRefr);
}

/******************************************************************************/
/* Private                       R e g F a i l                                */
/******************************************************************************/

bool XrdNetRefresh::RegFail(const char* why)
{
   eDest.Emsg("Refresh", "Peer cannot be registered;", why);
   return false;
}
  
/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

bool XrdNetRefresh::Register(XrdNetPeer& Peer)
{
   static unsigned int INum = 0;

// Make sure we have a valid hostname
//
   if (!Peer.InetName) return RegFail("hostname missing");

   if (index(Peer.InetName, '!')) return RegFail("invalid hostname");

// Make sure file discriptor is not negative
//
   if (Peer.fd < 0) return RegFail("invalid socket descriptor");

// Make sure the file descriptor is a UDP socket
//
   struct stat Stat;
   fstat(Peer.fd, &Stat);
   if (!S_ISSOCK(Stat.st_mode)) return RegFail("not a socket");

   int sockType;
   socklen_t stLen = sizeof(sockType);

   if (getsockopt(Peer.fd, SOL_SOCKET, SO_TYPE, (void*)&sockType, &stLen)
   || sockType != SOCK_DGRAM) return RegFail("not a UDP socket");

// Constrcut a new info structure to insert into the map
//
   RefInfo newRef(Peer.InetName, INum++);

// Add port number to the registration name (v4 and v6 ports are the same)
//
   newRef.destHN.append(":");
   newRef.destHN.append(std::to_string(ntohs(Peer.Inet.v4.sin_port)));

// Lock the map
//
   XrdSysMutexHelper mHelp(refMTX);

// Attempt to insert the element, verify that it happened
//
   auto ret = fd2Info.insert(std::pair<int, RefInfo>(Peer.fd, newRef));
   if (ret.second==false)
      {char buff[32];
       snprintf(buff, sizeof(buff), "(%d)", Peer.fd);
       eDest.Emsg("Refresh", "Peer cannot be registered"
                              "duplicate socket descriptor", buff);
       return false;
      }

// All done, it went well
//
   return true;
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/

void XrdNetRefresh::Start(XrdSysLogger* logP, XrdScheduler *sP)
{
// Set pointers we will need
//
   eDest.logger(logP);
   schedP = sP;

// We allocate an instance of this object so that we can schedule it.
//
   NetRefresh = new XrdNetRefresh();

// Now schedule it to refresh UDP destinations
//
   schedP->Schedule(NetRefresh, time(0)+udpRefr);
}  

/******************************************************************************/
/*                            U n R e g i s t e r                             */
/******************************************************************************/
  
void XrdNetRefresh::UnRegister(int fd)
{
   XrdSysMutexHelper mHelp(refMTX);

// Attempt to delete the fd from out set
//
   if (!fd2Info.erase(fd))
      {char buff[32];
       snprintf(buff, sizeof(buff), "%d)", fd);
       eDest.Emsg("Refresh", "Atempt to unregisted non-existent fd:",buff);
      }
}
  
/******************************************************************************/
/* Private:                      S e t D e s t                                */
/******************************************************************************/

bool XrdNetRefresh::SetDest(int fd, XrdNetSockAddr& netAddr, const char* hName,
                             bool newFam)
{
// If we haven't changed families, then we have merely changed addresses so
// all we need to co is change the connect address which is an atomic op.
//
   if (!newFam)
      {if (connect(fd, &netAddr.Addr, sizeof(netAddr.Addr)))
          {eDest.Emsg("Refresh", errno, "set new UDP address for", hName);
           return false;
          }
      }

// Since the family changed, we sill need to replace the whole socket
// definition with a new one. First establish what kind of socket is needed.
//
   int sFD, sProt = (netAddr.Addr.sa_family == AF_INET6 ? PF_INET6 : PF_INET);

// Create the new socket
//
   if ((sFD = XrdSysFD_Socket(sProt, SOCK_DGRAM, 0)) < 0)
      {eDest.Emsg("Refresh",errno,"create socket for new UDP address for",hName);
       return false;
      }

// Now set the destination address for this socket
//
   if (connect(sFD, &netAddr.Addr, sizeof(netAddr.Addr)))
      {eDest.Emsg("Refresh", errno, "init new UDP address for", hName);
       close(sFD);
       return false;
      }

// Displace the old socket with our new socket. This is an atomic operation.
//
   if (XrdSysFD_Dup2(sFD, fd) < 0)
      {eDest.Emsg("Refresh", errno, "replace old UDP address for", hName);
       close(sFD);
       return false;
      }

// We are succcessul
//
   close(sFD);
   return true;
}
  
/******************************************************************************/
/*                                U p d a t e                                 */
/******************************************************************************/
  
void XrdNetRefresh::Update()
{
   struct UpdInfo
         {XrdNetSockAddr destIP;
          unsigned int   Instance;
         };
   std::map<int, RefInfo> fd2Info_Copy;
   std::map<int, UpdInfo> fd2Info_Updt;

// Make a copy of our registry
//
   refMTX.Lock();
   fd2Info_Copy = fd2Info;
   refMTX.UnLock();

// For each entry in our local map, resolve the hostname and put the address
// in our update map. This prevents us from hold the registry lock across
// multiple DNS resolutions which may be lengthy.
//
   for (auto it = fd2Info_Copy.begin(); it != fd2Info_Copy.end(); ++it)
       {XrdNetAddr hAddr;
        const char* hName = it->second.destHN.c_str();
        const char* eText = hAddr.Set(hName, 0);

        if (eText) eDest.Emsg("Refresh", hName, "resolution failed;", eText);
           else {const XrdNetSockAddr* netIP = hAddr.NetAddr();
                 if (!netIP)
                     eDest.Emsg("Refresh","Unable to get addr of", hName);
                     else {UpdInfo newInfo = {*netIP, it->second.Instance};
                           fd2Info_Updt.insert(std::pair<int, UpdInfo>
                                               (it->first,newInfo));
                          }
                }
       }

// Now run through all of the updates to see if anything changed. We need
// to run with the registry locked to prevent an FD changes which we run.
//
   refMTX.Lock();
   int numDiff = 0, numUpdt = 0, numFail = 0;
   for (auto itu = fd2Info_Updt.begin(); itu != fd2Info_Updt.end(); ++itu)
       {auto itr = fd2Info.find(itu->first);
        if (itr != fd2Info.end()
        &&  itr->second.Instance == itu->second.Instance)
           {const char* hName = itr->second.destHN.c_str();
            XrdNetSockAddr curIP;                   
            socklen_t cSize =  sizeof(curIP.Addr);
            if (getpeername(itu->first, &curIP.Addr, &cSize))
               {eDest.Emsg("Refresh",errno,"get current peername of",hName); 
                numFail++;
                continue;
               }
            XrdNetUtils::IPComp result;
            result = XrdNetUtils::Compare(curIP, itu->second.destIP);
            if (result == XrdNetUtils::IPSame) continue;
            numDiff++;
            if (result != XrdNetUtils::IPDiff)
               {eDest.Emsg("Refresh", "IP family exception for", hName);
                numFail++;
                continue;
               }
            bool newFam = (result == XrdNetUtils::IPDFam);
            itu->second.destIP.v6.sin6_port = curIP.v6.sin6_port;
            if (SetDest(itu->first,itu->second.destIP,hName,newFam)) numUpdt++;
               else numFail++;
           }
       }
   refMTX.UnLock();

// Format the final message
//
   char mtext[128];
   snprintf(mtext, sizeof(mtext), "%d of %d IP changed: %d updt %d fail",
            numDiff, (int)fd2Info_Updt.size(), numUpdt, numFail);
   eDest.Emsg("Refresh", "Results:", mtext);

// Reschedule ourselves
//
   schedP->Schedule(NetRefresh, time(0)+udpRefr);
} 

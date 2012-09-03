/******************************************************************************/
/*                                                                            */
/*                     X r d F r c R e q A g e n t . c c                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdFrc/XrdFrcReqAgent.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

using namespace XrdFrc;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
char *XrdFrcReqAgent::c2sFN = 0;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdFrcReqAgent::XrdFrcReqAgent(const char *Me, int qVal)
              : Persona(Me),myName(""),theQ(qVal)
{
// Set default ping message
//
   switch(qVal)
         {case XrdFrcRequest::getQ: pingMsg = "!<\n"; break;
          case XrdFrcRequest::migQ: pingMsg = "!&\n"; break;
          case XrdFrcRequest::stgQ: pingMsg = "!+\n"; break;
          case XrdFrcRequest::putQ: pingMsg = "!>\n"; break;
          default:                  pingMsg = "!\n" ; break;
         }
}

/******************************************************************************/
/* Public:                           A d d                                    */
/******************************************************************************/
  
void XrdFrcReqAgent::Add(XrdFrcRequest &Request)
{

// Complete the request including verifying the priority
//
   if (Request.Prty > XrdFrcRequest::maxPrty)
      Request.Prty = XrdFrcRequest::maxPrty;
      else if (Request.Prty < 0)Request.Prty = 0;

// Add time and instance name
//
   Request.addTOD = time(0);
   if (myName) strlcpy(Request.iName, myName, sizeof(Request.iName));

// Now add it to the queue
//
   rQueue[static_cast<int>(Request.Prty)]->Add(&Request);

// Now wake the boss
//
   Ping();
}

/******************************************************************************/
/* Public:                           D e l                                    */
/******************************************************************************/
  
void XrdFrcReqAgent::Del(XrdFrcRequest &Request)
{
   int i;
  
// Remove all pending requests for this id
//
   for (i = 0; i <= XrdFrcRequest::maxPrty; i++) rQueue[i]->Can(&Request);
}

/******************************************************************************/
/* Public:                          L i s t                                   */
/******************************************************************************/
  
int XrdFrcReqAgent::List(XrdFrcRequest::Item *Items, int Num)
{
   char myLfn[8192];
   int i, Offs, n = 0;

// List entries in each priority queue
//
   for (i = 0; i <= XrdFrcRequest::maxPrty; i++)
       {Offs = 0;
        while(rQueue[i]->List(myLfn, sizeof(myLfn), Offs, Items, Num))
             {cout <<myLfn <<endl; n++;}
       }
// All done
//
   return n;
}

/******************************************************************************/
  
int XrdFrcReqAgent::List(XrdFrcRequest::Item *Items, int Num, int Prty)
{
   char myLfn[8192];
   int Offs, n = 0;

// List entries in each priority queue
//
   if (Prty <= XrdFrcRequest::maxPrty)
       {Offs = 0;
        while(rQueue[Prty]->List(myLfn, sizeof(myLfn), Offs, Items, Num))
             {cout <<myLfn <<endl; n++;}
       }

// All done
//
   return n;
}
  
/******************************************************************************/
/* Public:                       N e x t L F N                                */
/******************************************************************************/
  
int XrdFrcReqAgent::NextLFN(char *Buff, int Bsz, int Prty, int &Offs)
{
   static XrdFrcRequest::Item Items[1] = {XrdFrcRequest::getLFN};

// Return entry, if it exists
//
   return rQueue[Prty]->List(Buff, Bsz, Offs, Items, 1) != 0;
}

/******************************************************************************/
/*                                  P i n g                                   */
/******************************************************************************/

void XrdFrcReqAgent::Ping(const char *Msg)
{
   static XrdNetMsg udpMsg(&Say, c2sFN);
   static int udpOK = 0;
   struct stat buf;

// Send given message or default message based on our persona
//
   if (udpOK || !stat(c2sFN, &buf))
      {udpMsg.Send(Msg ? Msg : pingMsg); udpOK = 1;}
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
int XrdFrcReqAgent::Start(char *aPath, int aMode)
{
   XrdFrcRequest Request;
   const char *myClid;
   char buff[2048], *qPath;
   int i;

// Initialize the udp path for pings, if we have not done so
//
   if (!c2sFN)
      {sprintf(buff, "%sxfrd.udp", aPath);
       c2sFN = strdup(buff);
      }

// Get the instance name
//
   myName = XrdOucUtils::InstName(1);

// Generate the queue directory path
//
   if (!(qPath = XrdFrcUtils::makeQDir(aPath, aMode))) return 0;

// Initialize the registration entry and register ourselves
//
   if ((myClid = getenv("XRDCMSCLUSTERID")))
      {int Uid = static_cast<int>(geteuid());
       int Gid = static_cast<int>(getegid());
       memset(&Request, 0, sizeof(Request));
       strlcpy(Request.LFN, myClid, sizeof(Request.LFN));
       sprintf(Request.User,"%d %d", Uid, Gid);
       sprintf(Request.ID, "%d", static_cast<int>(getpid()));
       strlcpy(Request.iName, myName, sizeof(Request.iName));
       Request.addTOD = time(0);
       Request.Options = XrdFrcRequest::Register;
       Request.OPc = '@';
      }

// Initialize the request queues if all went well
//
   for (i = 0; i <= XrdFrcRequest::maxPrty; i++)
       {sprintf(buff, "%s%sQ.%d", qPath, Persona, i);
        rQueue[i] = new XrdFrcReqFile(buff, 1);
        if (!rQueue[i]->Init()) return 0;
        if (myClid) rQueue[i]->Add(&Request);
       }

// All done
//
   if (myClid) Ping();
   free(qPath);
   return 1;
}

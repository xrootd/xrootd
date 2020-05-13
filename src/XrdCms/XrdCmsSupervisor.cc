/******************************************************************************/
/*                                                                            */
/*                   X r d C m s S u p e r v i s o r . c c                    */
/*                                                                            */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"

#include "XrdCms/XrdCmsConfig.hh"
#include "XrdCms/XrdCmsProtocol.hh"
#include "XrdCms/XrdCmsSupervisor.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdNet/XrdNetSocket.hh"

#include "XrdSys/XrdSysError.hh"

using namespace XrdCms;

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

int      XrdCmsSupervisor::superOK = 0;

XrdInet *XrdCmsSupervisor::NetTCPr = 0;

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdCmsSupervisor::Init(const char *AdminPath, int AdminMode)
{
   char spbuff[1024];

// Create the supervisor unix domain socket. We use this for the redirector
// assigned to this supervisor node (one one redirector allowed)
//
   if (!XrdNetSocket::socketPath(&Say, spbuff, AdminPath,
      "olbd.super", AdminMode | S_IFSOCK)) return 1;

// Create a new network suitable for use with XrdLink objects
//
   if (!(NetTCPr = new XrdInet(&Say)))
      {Say.Emsg("Supervisor","Unable to create supervisor interface.");
       return 0;
      }

// Set out domain name for this network
//
   if (Config.myDomain) NetTCPr->setDomain(Config.myDomain);

// Bind the unix domain path to the network
//
   if (NetTCPr->Bind(spbuff, "tcp")) return 0;

// We need to force the minimum number of subscribers to be one and never a
// percentage, regardless of what was specified in the config file. This is
// because supervisors may loose their subscribers in unusual ways. We also
// set the drop delay to zero to immediately drop servers from the config to
// avoid stalling clients at a supervisor node.
//
   Config.SUPCount = 1;
   Config.SUPLevel = 0;
   Config.DRPDelay = 0;

// All done
//
   superOK = 1;
   return 1;
}
  
/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/

void XrdCmsSupervisor::Start()
{
   XrdCmsProtocol *pP;
   XrdLink        *lP;

// Accept single connections and dispatch the supervisor data interface
//
   while(1) 
        if ((lP = NetTCPr->Accept(XRDNET_NODNTRIM)))
           {if ((pP = XrdCmsProtocol::Alloc("redirector")))
               {lP->setProtocol((XrdProtocol *)pP);
                pP->Process(lP);
                lP->Close();
               }
           }
}

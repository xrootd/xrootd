/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d P l u g i n . c c                     */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
 
#include "XrdVersion.hh"

#include "XrdXrootd/XrdXrootdProtocol.hh"

/******************************************************************************/
/*                       P r o t o c o l   L o a d e r                        */
/*                        X r d g e t P r o t o c o l                         */
/******************************************************************************/
  
// This protocol is defined in a shared library. The interface below is used
// by the protocol driver to obtain a copy of the protocol object that can be
// used to decide whether or not a link is talking a particular protocol. This
// definition is used when XRootD is loaded as an ancillary protocol.
//
XrdVERSIONINFO(XrdgetProtocol,xrootd);

extern "C"
{
XrdProtocol *XrdgetProtocol(const char *pname, char *parms,
                              XrdProtocol_Config *pi)
{
   XrdProtocol *pp = 0;
   const char *txt = "completed.";

// Put up the banner
//
   pi->eDest->Say("Copr.  2012 Stanford University, xrootd protocol "
                   kXR_PROTOCOLVSTRING, " version ", XrdVERSION);
   pi->eDest->Say("++++++ xrootd protocol initialization started.");

// Return the protocol object to be used if static init succeeds
//
   if (XrdXrootdProtocol::Configure(parms, pi))
      pp = (XrdProtocol *)new XrdXrootdProtocol();
      else txt = "failed.";
    pi->eDest->Say("------ xrootd protocol initialization ", txt);
   return pp;
}
}

/******************************************************************************/
/*                                                                            */
/*           P r o t o c o l   P o r t   D e t e r m i n a t i o n            */
/*                    X r d g e t P r o t o c o l P o r t                     */
/******************************************************************************/

// This function is called early on to determine the port we need to use. The
// default is ostensibly 1094 but can be overidden; which we allow.
//
XrdVERSIONINFO(XrdgetProtocolPort,xrootd);

extern "C"
{
int XrdgetProtocolPort(const char *pname, char *parms, XrdProtocol_Config *pi)
{

// Figure out what port number we should return. In practice only one port
// number is allowed. However, we could potentially have a clustered port
// and several unclustered ports. So, we let this practicality slide.
//
   if (pi->Port < 0) return 1094;
   return pi->Port;
}
}

/******************************************************************************/
/*                                                                            */
/*                         X r d V o m s g s i . c c                          */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#include "XrdVomsFun.hh"

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

namespace
{
XrdVomsFun *vomsFun = 0;
}
  
/******************************************************************************/
/*               X R o o t D   P l u g i n   I n t e r f a c e                */
/******************************************************************************/
/******************************************************************************/
/*                      X r d S e c g s i V O M S F u n                       */
/******************************************************************************/
  
// For historical reason this externally loadable function does not follow
// the normal naming convention to be backward compatible. We should fix it.
//
extern "C"
{
int XrdSecgsiVOMSFun(XrdSecEntity &ent)
{
// Make sure we were initialized. If so, invoke the function and return result.
//
   return (vomsFun ? vomsFun->VOMSFun(ent) : -1);
}
}

/******************************************************************************/
/*                     X r d S e c g s i V O M S I n i t                      */
/******************************************************************************/
  
// Init the relevant parameters from a dedicated config parameter
//
extern "C"
{
int XrdSecgsiVOMSInit(const char *cfg)
{
   static XrdSysLogger gLogger;
   static XrdSysError gDest(&gLogger, "XrdVoms");

// Allocate a new Voms object
//
   vomsFun = new XrdVomsFun(gDest);

// Initialize it. There is no error return to speak of.
//
   return vomsFun->VOMSInit(cfg);
}
}

#ifndef __XRDSYSFATTR_HH__
#define __XRDSYSFATTR_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s F A t t r . h h                         */
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

#include "XrdSys/XrdSysXAttr.hh"

//------------------------------------------------------------------------------
//! This class provides an internal interface to handle extended file attributes
//! either via a default implementation or an external plugin.
//------------------------------------------------------------------------------

class XrdSysFAttr : public XrdSysXAttr
{
public:

//------------------------------------------------------------------------------
//! Xat    points to the plugin to be used for all operations. The methods
//!        inherited from XrdSysXAttr cannot be directly invoked. Instead,
//!        use XrdSysFAttr::Xat-><any XrdSysXAttr public method>. All static
//!        methods here, however, can be directly invoked.
//------------------------------------------------------------------------------

static XrdSysXAttr *Xat;

//------------------------------------------------------------------------------
//! Establish a plugin that is to replace the builtin extended attribute
//! processing methods.
//!
//! @param  xaP   -> To an instance of an XrdSysXAttr object that is to replace
//!                  the builtin object that processes extended attributes;
//------------------------------------------------------------------------------

static void  SetPlugin(XrdSysXAttr *xaP);

//------------------------------------------------------------------------------
//! Constructor & Destructor
//------------------------------------------------------------------------------

             XrdSysFAttr() {}
            ~XrdSysFAttr() {}

//------------------------------------------------------------------------------
//! The following methods are inherited from the base class as private methods.
//------------------------------------------------------------------------------

private:
       int Del(const char *Aname, const char *Path, int fd=-1);

       void Free(AList *aPL);

       int Get(const char *Aname, void *Aval, int Avsz,
               const char *Path,  int fd=-1);

       int List(AList **aPL, const char *Path, int fd=-1, int getSz=0);

       int Set(const char *Aname, const void *Aval, int Avsz,
               const char *Path,  int fd=-1,  int isNew=0);

       int Diagnose(const char *Op, const char *Var, const char *Path, int ec);

       AList *getEnt(const char *Path,  int    fd,
                     const char *Aname, AList *aP, int *msP);
};
#endif

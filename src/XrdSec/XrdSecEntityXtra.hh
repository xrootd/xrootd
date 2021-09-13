#ifndef __SEC_ENTITYXTRA_H__
#define __SEC_ENTITYXTRA_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d S e c E n t i t y X t r a . h h                    */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <map>
#include <cstring>
#include <vector>

#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdSecEntityXtra : public XrdSecEntityAttr
{
public:

XrdSysMutex                        xMutex;

std::vector<XrdSecAttr *>          attrVec;

std::map<std::string, std::string> attrMap;

void     Reset();

         XrdSecEntityXtra() : XrdSecEntityAttr(this) {}
        ~XrdSecEntityXtra() {Reset();}
};
#endif

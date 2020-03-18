#ifndef __XRDOUCPINOBJECT_HH__
#define __XRDOUCPINOBJECT_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c P i n O b j e c t . h h                     */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/*! The XrdOucPinObject defines a generic interface to obtain an instance of a
    plugin. Post R5 plugins must have an instance of this class in the shared
    library that implements the plugin named in the template parameter. The
    plugin handler calls getInstance() to obtain an actual instance of it.
*/

class XrdOucEnv;
class XrdOucLogger;

template<class T>
class XrdOucPinObject
{
public:

//------------------------------------------------------------------------------
//! Get the an instance of a plugin.
//!
//! @param parms  Pointer to any parameters, may be nil or the null string.
//! @param envR   Reference to the environment. If the server was started with
//!               a configuration file then key "configFN" holds its path.
//! @param logR   Pointer to logging object that should be assocaited with
//!               and XrdSysError object to relay messages.
//! @param prevP  Pointer to the previous instance if stacked, else nil.
//------------------------------------------------------------------------------

virtual
T              *getInstance(const char   *parms,
                            XrdOucEnv    &envR,
                            XrdSysLogger &logR,
                            T            *prevP) = 0;

//------------------------------------------------------------------------------
//! Constructor & Destructor
//------------------------------------------------------------------------------

                XrdOucPinObject() {}

virtual        ~XrdOucPinObject() {}
};
#endif

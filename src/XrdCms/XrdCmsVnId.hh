#ifndef __XRDCMSVNID_H__
#define __XRDCMSVNID_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d C m s V n I d . h h                          */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//------------------------------------------------------------------------------
//! Function XrdCmsgetVnId is used to obtain the unique node identification.
//! This plug-in is specified by the 'all.vnidlib' directive and when present
//! uses the plug-in to obtain the node identification. This ID is used to
//! track the node instead of the node's host name or IP address; essentially
//! creating a virtual network . This plugin is meant for container and
//! virtualization frameworks that assign an arbitrary host name or IP addresses
//! to a container or VM on each start-up; making host name or IP address an
//! unreliable tracking mechanism. Hence, nodes are tracked within a vrtual
//! network composed of the node ID and internal cluster ID.
//------------------------------------------------------------------------------

/******************************************************************************/
/*                         X r d C m s g e t V n I d                          */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain node identification.
//!
//! This extern "C" function is called to obtain the node identification.
//! It need not be thread safe as it is called only once during initialization.
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  confg -> Name of the configuration file that was used.
//! @param  parms -> Argument string specified on the vnidlib directive. It is
//!                  a null string if no parms exist.
//! @param  nRole -> The role this node has, as follow:
//!                  'm' - manager, 's' - server, 'u' - supervisor
//!                  If the letter is upper case, then it's a proxy role.
//! @param  mlen  -> The maximum length the return string may have. Returning
//!                  a string longer than mlen aborts initialization.
//!
//! @return Success: A string containing the node identification.
//!         Failure: A null string which causes initialization to fail.
//------------------------------------------------------------------------------

#define XrdCmsgetVnIdArgs XrdSysError       &eDest, \
                          const std::string &confg, \
                          const std::string &parms, \
                          char               nRole, \
                          int                mlen

/*! Declare this function as follows in you shared library:

extern "C" std::string XrdCmsgetVnId(XrdCmsgetVnIdArgs) {...}
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdCmsgetNidName,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif

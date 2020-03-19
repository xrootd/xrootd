#ifndef __XRDOFSFSCTL_PI_H__
#define __XRDOFSFSCTL_PI_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d O f s F S c t l _ P I . h h                      */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//! Class XrdOfsFSctl is used to customize the FSCtl() file system call. It is
//! loaded in response to the ofs.ctllib directive.

class XrdAccAuthorize;
class XrdCmsClient;
class XrdOss;
class XrdOucEnv;
class XrdOucErrInfo;
class XrdSecEntity;
class XrdSfsFile;
class XrdSfsFileSystem;
class XrdSfsFSctl;
class XrdSysError;
  
/******************************************************************************/
/*                           X r d O f s F S C t l                            */
/******************************************************************************/
  
class XrdOfsFSctl_PI
{
public:
friend class XrdOfsConfigPI;

//-----------------------------------------------------------------------------
//! The Plugins struct is used to pass plugin pointers to configure.
//-----------------------------------------------------------------------------

struct Plugins
      {XrdAccAuthorize  *autPI;    //!< -> Authorization plugin
       XrdCmsClient     *cmsPI;    //!< -> Cms client object generator plugin
       XrdOss           *ossPI;    //!< -> Oss plugin
       XrdSfsFileSystem *sfsPI;    //!< -> Sfs plugin (a.k.a. ofs)
      };

//-----------------------------------------------------------------------------
//! Configure plugin.
//!
//! @param  CfgFN  - Path of the configuration file.
//! @param  Parms  - Any parameters specified on the directive (may be null).
//! @param  envP   - Pointer to environmental information
//! @param  plugs  - Reference to the struct containing plugin pointers.
//!                  Unloaded plugins have a nil pointer.
//!
//! @return True upon success and false otherwise.
//-----------------------------------------------------------------------------

virtual bool           Configure(const char    *CfgFN,
                                 const char    *Parms,
                                 XrdOucEnv     *envP,
                                 const Plugins &plugs) {return true;}

//-----------------------------------------------------------------------------
//! Perform a file control operation
//!
//! @param  cmd    - The operation to be performed:
//!                  SFS_FCTL_SPEC1    Return Implementation Dependent Data
//! @param  alen   - The length of args.
//! @param  args   - Arguments specific to cmd.
//!                  SFS_FCTL_SPEC1    Unscreened args string.
//! @param  file   - Reference to the target file object.
//! @param  eInfo  - The object where error info or results are to be returned.
//! @param  client - Client's identify (see common description).
//!
//! @return SFS_OK   a null response is sent.
//!         SFS_DATA error.code    length of the data to be sent.
//!                  error.message contains the data to be sent.
//!         o/w      one of SFS_ERROR, SFS_REDIRECT, or SFS_STALL.
//-----------------------------------------------------------------------------

virtual int            FSctl(const int               cmd,
                                   int               alen,
                             const char             *args,
                                   XrdSfsFile       &file,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client = 0) = 0;

//-----------------------------------------------------------------------------
//! Perform a filesystem control operation (version 2)
//!
//! @param  cmd    - The operation to be performed:
//!                  SFS_FSCTL_PLUGIN  Return Implementation Dependent Data v1
//!                  SFS_FSCTL_PLUGIO  Return Implementation Dependent Data v2
//! @param  args   - Arguments specific to cmd.
//!                  SFS_FSCTL_PLUGIN  path and opaque information, fileP == 0
//!                  SFS_FSCTL_PLUGIO  Unscreened argument string,  fileP == 0
//! @param  eInfo  - The object where error info or results are to be returned.
//! @param  client - Client's identify (see common description).
//!
//! @return SFS_OK   a null response is sent.
//!         SFS_DATA error.code    length of the data to be sent.
//!                  error.message contains the data to be sent.
//!         o/w      one of SFS_ERROR, SFS_REDIRECT, or SFS_STALL.
//-----------------------------------------------------------------------------

virtual int            FSctl(const int               cmd,
                                   XrdSfsFSctl      &args,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client = 0) = 0;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

             XrdOfsFSctl_PI() : prvPI(0), eDest(0) {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdOfsFSctl_PI() {}

protected:

XrdOfsFSctl_PI  *prvPI; // Stacked CTL plugin behind this one for forwarding
                        // If none, then the pointer is zero.
XrdSysError     *eDest; // Message logging object to be used for messages.
};

/******************************************************************************/
/*                    X r d O f s F S c t l   P l u g i n                     */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! To create a loadable XrdfsFSCtl plugin, simply derive your implementation
//! class from the XrdOfsFSctl_PI class. Then declare an instance of that class
//! at file level with the name XrdOfsFSctl. An example follows.
//------------------------------------------------------------------------------

/*!  class myFSCtl : public XrdOfsFSctl_PI {. . . .};

     myFSCtl XrdOfsFSctl;
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOfsFSctl,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif

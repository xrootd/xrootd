#ifndef __XRDOUCNAME2NAME_H__
#define __XRDOUCNAME2NAME_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c n a m e 2 n a m e . h h                     */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>
#include <vector>

/******************************************************************************/
/*                       X r d O u c N a m e 2 N a m e                        */
/******************************************************************************/

//! Class XrdoucName2Name must be used for creating a name translation plug-in.
//! This plug-in is specified by the 'oss.namelib' directive and when present
//! makes the default oss plug-in load the plug-in shared library, locate the
//! XrdOucgetName2Name function within and use it to obtain an instance of the
//! XrdOucName2Name object to perform name translation prior to all subsequent
//! storage system calls. The companion object, XrdOucName2NameVec, should
//! also be defined in the same shared library (see the class definition below).
  
class XrdOucName2Name
{
public:

//------------------------------------------------------------------------------
//! Map a logical file name to a physical file name.
//!
//! @param  lfn   -> Logical file name.
//! @param  buff  -> Buffer where the physical file name of an existing file is
//!                  to be placed. It must end with a null byte.
//! @param  blen     The length of the buffer.
//!
//! @return Success: Zero.
//!         Failure: An errno number describing the failure; typically
//!                  EINVAL       - The supplied lfn is invalid.
//!                  ENAMETOOLONG - The buffer is too small for the pfn.
//------------------------------------------------------------------------------

virtual int lfn2pfn(const char *lfn, char *buff, int blen) = 0;

//------------------------------------------------------------------------------
//! Map a logical file name to the name the file would have in a remote storage
//! system (e.g. Mass Storage System at a remote location).
//!
//! @param  lfn   -> Logical file name.
//! @param  buff  -> Buffer where the remote file name is to be placed. It need
//!                  not actually exist in that location but could be created
//!                  there with that name. It must end with a null byte.
//! @param  blen     The length of the buffer.
//!
//! @return Success: Zero.
//!         Failure: An errno number describing the failure; typically
//!                  EINVAL       - The supplied lfn is invalid.
//!                  ENAMETOOLONG - The buffer is too small for the pfn.
//------------------------------------------------------------------------------

virtual int lfn2rfn(const char *lfn, char *buff, int blen) = 0;

//------------------------------------------------------------------------------
//! Map a physical file name to it's logical file name.
//!
//! @param  pfn   -> Physical file name. This is always a valid name of either
//!                  an existing file or a file that could been created.
//! @param  buff  -> Buffer where the logical file name is to be placed. It need
//!                  not actually exist but could be created with that name.
//!                  It must end with a null byte.
//! @param  blen     The length of the buffer.
//!
//! @return Success: Zero.
//!         Failure: An errno number describing the failure; typically
//!                  EINVAL       - The supplied lfn is invalid.
//!                  ENAMETOOLONG - The buffer is too small for the pfn.
//------------------------------------------------------------------------------

virtual int pfn2lfn(const char *pfn, char *buff, int blen) = 0;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

             XrdOucName2Name() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdOucName2Name() {}
};

/******************************************************************************/
/*                    X r d O u c N a m e 2 N a m e V e c                     */
/******************************************************************************/

//! Class XrdOucName2NameVec must be used to define a companion name translation
//! mechanism. It is optional but highly recommended and may in fact be required
//! by certain statlib plug-ins specific by the 'oss.statlib' directive. Refer
//! to plug-in documentation to see if it requires this form of name2name
//! translator. This translator should return all possible translations of a
//! given logical file name. After an instance of the XrdOucName2Name
//! translator is obtained (which implies it's full initilization) the default
//! oss plug-in check if the symbol 'Name2NameVec' is present in the shared
//! library. If it does, it obtains the contents of the symbol which should be
//! a pointer to an object derived from the following class. That object is
//! used to obtain a list of possible name translations. Initialization is
//! is simplified if your implementation inherits XrdOucName2Name as well as
//! XrdOucName2Namevec. The symbol that contains the pointer must be defined
//! at file level as follows:

//! XrdOucName2NameVec *Name2NameVec;

//! It should be set during XrdOucName2Name initialization to point to an
//! instance of the object. The methods defined for this class must be
//! thread-safe. The default XrdOucName2Name translator also includes the
//! XrdOucName2NameVec translator.
  
class XrdOucName2NameVec
{
public:

//------------------------------------------------------------------------------
//! Map a logical file name to all of its possible physical file names.
//!
//! @param  lfn   -> Logical file name.
//!
//! @return Success: Pointer to a vector of strings of physical file names.
//!         Failure: A nil pointer indicating that no translation exists.
//------------------------------------------------------------------------------

virtual std::vector<std::string *> *n2nVec(const char *lfn)=0;

//------------------------------------------------------------------------------
//! Release all storage occupied by the vector returned by n2nVec().
//!
//! @param  nvP   -> Vector returned by n2nVec().
//------------------------------------------------------------------------------

virtual void Recycle(std::vector<std::string *> *nvP)
                    {if (nvP)
                        {for (unsigned int i = 0; i < nvP->size(); i++)
                             {delete (*nvP)[i];}
                         delete nvP;
                        }
                    }

//------------------------------------------------------------------------------
//! Constructor and Destructor
//------------------------------------------------------------------------------

             XrdOucName2NameVec() {}
virtual     ~XrdOucName2NameVec() {}
};

/******************************************************************************/
/*                    X r d O u c g e t N a m e 2 N a m e                     */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the XrdOucName2Name object.
//!
//! This extern "C" function is called when a shared library plug-in containing
//! implementation of this class is loaded. It must exist in the shared library
//! and must be thread-safe.
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  confg -> Name of the configuration file that was used. This pointer
//!                  may be null though that would be impossible.
//! @param  parms -> Argument string specified on the namelib directive. It may
//!                  be null or point to a null string if no parms exist.
//! @param  lroot -> The path specified by the localroot directive. It is a
//!                  null pointer if the directive was not specified.
//! @param  rroot -> The path specified by the remoteroot directive. It is a
//!                  null pointer if the directive was not specified.
//!
//! @return Success: A pointer to an instance of the XrdOucName2Name object.
//!         Failure: A null pointer which causes initialization to fail.
//!
//! The Name2Name object is used frequently in the course of opening files
//! as well as other meta-file operations (e.g., stat(), rename(), etc.).
//! The algorithms used by this object *must* be efficient and speedy;
//! otherwise system performance will be severely degraded.
//------------------------------------------------------------------------------

class XrdSysError;

#define XrdOucgetName2NameArgs XrdSysError       *eDest, \
                               const char        *confg, \
                               const char        *parms, \
                               const char        *lroot, \
                               const char        *rroot

extern "C" XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs);

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOucgetName2Name,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif

#ifndef __XRDSYSXATTR_HH__
#define __XRDSYSXATTR_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s X A t t r . h h                         */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
//! This pure abstract class defines the extended attribute interface and is
//! used by extended attribute plugin writers to implement extended attribute
//! handling. The plugin is loaded via the ofs.xattrlib directive.
//------------------------------------------------------------------------------

class XrdSysError;

class XrdSysXAttr
{
public:
//------------------------------------------------------------------------------
//! Definition of a structure to hold an attribute name and the size of the
//! name as well as the size of its associated value. The structure is a list
//! and is used as an argument to Free() and is returned by List(). The size of
//! the struct is dynamic and should be sized to hold all of the information.
//------------------------------------------------------------------------------

struct AList
      {AList *Next;    //!< -> next element.
       int    Vlen;    //!<   The length of the attribute value;
       int    Nlen;    //!<   The length of the attribute name that follows.
       char   Name[1]; //!<   Start of the name (size of struct is dynamic)
      };

//------------------------------------------------------------------------------
//! Copy one or all extended attributes from one file to another (a default
//! implementation is supplied).
//!
//! @param  iPath  -> Path of the file whose attribute(s) are to be copied.
//! @param  iFD       If >=0 is the file descriptor of the opened source file.
//! @param  oPath  -> Path of the file to receive the extended attribute(s).
//!                   Duplicate attributes are replaced.
//! @param  oFD       If >=0 is the file descriptor of the opened target file.
//! @param  Aname  -> if nil, the all of the attributes of the source file are
//!                   copied. Otherwise, only the attribute name pointed to by
//!                   Aname is copied. If Aname does not exist or extended
//!                   attributes are not supported, the operation succeeds by
//!                   copying nothing.
//!
//! @return =0     Attribute(s) successfully copied, did not exist, or extended
//!                attributes are not supported for source or target.
//! @return <0     Attribute(s) not copied, the return value is -errno that
//!                describes the reason for the failure.
//------------------------------------------------------------------------------

virtual int  Copy(const char *iPath, int iFD, const char *oPath, int oFD,
                  const char *Aname=0);

//------------------------------------------------------------------------------
//! Remove an extended attribute.
//!
//! @param  Aname  -> The attribute name.
//! @param  Path   -> Path of the file whose attribute is to be removed.
//! @param  fd        If >=0 is the file descriptor of the opened subject file.
//!
//! @return =0     Attribute was successfully removed or did not exist.
//! @return <0     Attribute was not removed, the return value is -errno that
//!                describes the reason for the failure.
//------------------------------------------------------------------------------

virtual int  Del(const char *Aname, const char *Path, int fd=-1) = 0;

//------------------------------------------------------------------------------
//! Release storage occupied by the Alist structure returned by List().
//!
//! @param  aPL    -> The first element of the AList structure.
//------------------------------------------------------------------------------

virtual void Free(AList *aPL) = 0;

//------------------------------------------------------------------------------
//! Get an attribute value and its size.
//!
//! @param  Aname  -> The attribute name.
//! @param  Aval   -> Buffer to receive the attribute value.
//! @param  Avsz      Length of the buffer in bytes. Only up to this number of
//!                   bytes should be returned. However, should Avsz be zero
//!                   the the size of the attribute value should be returned
//!                   and the Aval argument should be ignored.
//! @param  Path   -> Path of the file whose attribute is to be fetched.
//! @param  fd     -> If >=0 is the file descriptor of the opened subject file.
//!
//! @return >0     The number of bytes placed in Aval. However, if avsz is zero
//!                then the value is the actual size of the attribute value.
//! @return =0     The attribute does not exist.
//! @return <0     The attribute value could not be returned. The returned
//!                value is -errno describing the reason.
//------------------------------------------------------------------------------

virtual int  Get(const char *Aname, void *Aval, int Avsz,
                 const char *Path,  int fd=-1) = 0;

//------------------------------------------------------------------------------
//! Get all of the attributes associated with a file.
//!
//! @param  aPL    -> the pointer to hold the first element of AList. The
//!                   storage occupied by the returned AList must be released
//!                   by calling Free().
//! @param  Path   -> Path of the file whose attributes are t be returned.
//! @param  fd     -> If >=0 is the file descriptor of the opened subject file.
//! @param  getSz     When != 0 then the size of the maximum attribute value
//!                   should be returned. Otherwise, upon success 0 is returned.
//!
//! @return >0     Attributes were returned and aPL points to the first
//!                attribute value.  The returned value is the largest size
//!                of an attribute value encountered (getSz != 0).
//! @return =0     Attributes were returned and aPL points to the first
//!                attribute value (getSz == 0).
//! @return <0     The attribute values could not be returned. The returned
//!                value is -errno describing the reason.
//------------------------------------------------------------------------------

virtual int  List(AList **aPL, const char *Path, int fd=-1, int getSz=0) = 0;

//------------------------------------------------------------------------------
//! Set an attribute.
//!
//! @param  Aname  -> The attribute name.
//! @param  Aval   -> Buffer holding the attribute value.
//! @param  Avsz      Length of the buffer in bytes. This is the length of the
//!                   attribute value which may contain binary data.
//! @param  Path   -> Path of the file whose attribute is to be set.
//! @param  fd     -> If >=0 is the file descriptor of the opened subject file.
//! @param  isnew     When !0 then the attribute must not exist (i.e. new).
//!                   Otherwise, if it does exist, the value is replaced. In
//!                   either case, if it does not exist it should be created.
//!
//! @return =0     The attribute was successfully set.
//! @return <0     The attribute values could not be set. The returned
//!                value is -errno describing the reason.
//------------------------------------------------------------------------------

virtual int  Set(const char *Aname, const void *Aval, int Avsz,
                 const char *Path,  int fd=-1,  int isNew=0) = 0;

//------------------------------------------------------------------------------
//! Establish the error message routing. Unless it's established, no messages
//! should be produced. A default impleentation is supplied.
//!
//! @param  errP   -> Pointer to the error message object. If it is a nil
//!                   pointer, no error messages should be produced.
//!
//! @return The previous setting.
//------------------------------------------------------------------------------

virtual XrdSysError *SetMsgRoute(XrdSysError *errP);

//------------------------------------------------------------------------------
//! Constructor and Destructor
//------------------------------------------------------------------------------

         XrdSysXAttr() : Say(0) {}
virtual ~XrdSysXAttr() {}

protected:

XrdSysError *Say;
};

/******************************************************************************/
/*       X r d S y s X A t t r   O b j e c t   I n s t a n t i a t o r        */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Get an instance of a configured XrdSysXAttr object.
//!
//! @param  errP       -> Error message object for error messages.
//! @param  config_fn  -> The name of the config file.
//! @param  parms      -> Any parameters specified on the ofs.xattrlib
//!                       directive. If there are no parameters parms may be 0.
//!
//! @return Success:   -> an instance of the XrdSysXattr object to be used.
//!         Failure:      Null pointer which causes initialization to fail.
//!
//! The object creation function must be declared as an extern "C" function
//! in the plug-in shared library as follows:
//------------------------------------------------------------------------------
/*!
    extern "C" XrdSysXAttr *XrdSysGetXAttrObject(XrdSysError  *errP,
                                                 const char   *config_fn,
                                                 const char   *parms);
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdSysGetXAttrObject,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif

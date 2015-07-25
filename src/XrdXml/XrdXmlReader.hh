#ifndef __XRDXMLREADER_HH__
#define __XRDXMLREADER_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d X m l R e a d e r . h h                        */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
//-----------------------------------------------------------------------------
//! The XrdXmlReader object provides a virtual interface to read xml files,
//! irrespective of the underlying mplementation. Obtain an XML reader using
//! GetRead(). You may also wish to call Init() prior to obtaining a reader
//! in multi-threader applications. As some implementations may not be MT-safe.
//! See GetReader() for more information.
//-----------------------------------------------------------------------------

class XrdXmlReader
{
public:

//-----------------------------------------------------------------------------
//! Get attributes from an XML tag. GetAttributes() should only be called after
//! a successful GetElement() call.
//!
//! @param  aname    Pointer to an array of attribute names whose values are
//!                  to be returned. The last entry in the array must be nil.
//!
//! @param  aval     Pointer to an array where the corresponding attribute
//!                  values are to be placed in 1-to-1 correspondence. The
//!                  values must be freed using free().
//!
//! @return true     One or more attributes have been returned.
//!         false    No specified attributes were found.
//-----------------------------------------------------------------------------

virtual bool    GetAttributes(const char **aname, char **aval)=0;

//-----------------------------------------------------------------------------
//! Find an XML tag element.
//!
//! @param  ename    Pointer to an array of tag names any of which should be
//!                  searched for. The last entry in the array must be nil.
//!                  The first element of the array should contain the name of
//!                  the context tag. Elements are searched only within the
//!                  scope of that tag. When searching for the first desired
//!                  tag, use a null string to indicate document scope.
//!
//! @param  reqd     When true one of the tag elements listed in ename must be
//!                  found otherwise an error is generated.
//!
//! @return =0       No specified tag was found. Note that this corresponds to
//!                  encountering the tag present in ename[0], i.e. scope end.
//!         >0       A tag was found, the return value is the index into ename
//!                  that corresponds to the tag's name.
//-----------------------------------------------------------------------------

virtual int     GetElement(const char **ename, bool reqd=false)=0;

//-----------------------------------------------------------------------------
//! Get the description of the last error encountered.
//!
//! @param  ecode    The error code associated with the error.
//!
//! @return Pointer to text describing the error. The text may be destroyed on a
//!         subsequent call to any other method. Otherwise it is stable. A nil
//!         pointer indicates that no error is present.
//-----------------------------------------------------------------------------
virtual
const char     *GetError(int &ecode)=0;

//-----------------------------------------------------------------------------
//! Get a reader object to parse an XML file.
//!
//! @param  fname    Pointer to the filepath of the file to be parsed.
//!
//! @param  enc      Pointer to the encoding specification. When nil, UTF-8 is
//!                  used. Currently, this parameter is ignored.
//!
//! @param  impl     Pointer to the desired implementation. When nil, the
//!                  default implementation, tinyxml, is used. The following
//!                  are supported
//!
//!                  tinyxml   - builtin xml reader. Each instance is independent
//!                              Since it builds a full DOM tree in memory, it
//!                              is only good for small amounts of xml. Certain
//!                              esoteric xml features are not supported.
//!
//!                  libxml2   - full-fledged xml reader. Instances are not
//!                              independent if multiple uses involve setting
//!                              callbacks, allocators, or I/O overrides. For
//!                              MT-safeness, it must be initialized in the
//!                              main thread (see Init() below). It is used in
//!                              streaming mode and is good for large documents.
//!
//!
//! @return !0       Pointer to an XML reader object.
//! @return =0       An XML reader object could not be created; errno holds
//!                  the error code of the reason.
//-----------------------------------------------------------------------------
static
XrdXmlReader   *GetReader(const char *fname,
                          const char *enc=0, const char *impl=0);

//-----------------------------------------------------------------------------
//! Get the text portion of an XML tag element. GetText() should only be called
//! after a successful call to GetElement() with a possibly intervening call
//! to GetAttributes().
//!
//! @param  ename    Pointer to the corresponding tag name.
//!
//! @param  reqd     When true text must exist and not be null. Otherwise, an
//!                  error is generated if the text is missing or null.
//!
//! @return =0       No text found.
//! @return !0       Pointer  to the tag's text field. It must be free using
//!                  free().
//-----------------------------------------------------------------------------

virtual char   *GetText(const char *ename, bool reqd=false)=0;

//-----------------------------------------------------------------------------
//! Preinitialze the desired implementation for future use. This is meant to be
//! used in multi-threaded applications, as some implementation must be
//! initialized using the main thread before spawning other threads. An exmaple
//! is libxml2 which is generally MT-unsafe unles preinitialized.
//!
//! @param  impl     Pointer to the desired implementation. When nil, the
//!                  default implementation is used. Currently, only "libxml2"
//!                  and "tinyxml" are supported.
//!
//! @return true     Initialization suceeded.
//! @return false    Initialization failed, errno has the reason.
//-----------------------------------------------------------------------------

static bool     Init(const char *impl=0);

//-----------------------------------------------------------------------------
//! Constructor & Destructor
//-----------------------------------------------------------------------------

                XrdXmlReader() {}
virtual        ~XrdXmlReader() {}

private:

};
#endif

#ifndef __XRDOUCFILEINFO_HH__
#define __XRDOUCFILEINFO_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d O u c F i l e I n f o . h h                      */
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

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
  
//-----------------------------------------------------------------------------
//! The XrdOucFileInfo object provides a uniform interface to describe a file
//! resource that may be available from one or more locations.
//-----------------------------------------------------------------------------

class XrdOucFIHash;
class XrdOucFIUrl;

class XrdOucFileInfo
{
public:

//-----------------------------------------------------------------------------
//! Add a digest to the file descriptions.
//!
//! @param  hname    Poiner to hash name.
//! @param  hval     Poiner to hash value.
//-----------------------------------------------------------------------------

void            AddDigest(const char *hname, const char *hval);

//-----------------------------------------------------------------------------
//! Add a url to the file descriptions.
//!
//! @param  url     Poiner to file url.
//! @param  cntry   Poiner to the optional 2-char null terminated country code.
//!                 Upper case country codes are converted to lower case.
//! @param  prty    Selection priority. If less than 0 it is set to zero. Urls
//!                 are placed in increasing prty order (0 is top priority).
//! @param  fifo    When true, the location is placed at the end of locations of
//!                 equal pririoty. Otherwise, is is placed at the head.
//-----------------------------------------------------------------------------

void            AddUrl(const char *url, const char *cntry=0,
                       int prty=0, bool fifo=true);

//-----------------------------------------------------------------------------
//! Add target filename to the file descriptions.
//!
//! @param  filename     Poiner to file name.
//-----------------------------------------------------------------------------

void            AddFileName(const char * filename);

//-----------------------------------------------------------------------------
//! Add logical filename to the file descriptions.
//!
//! @param  lfn          Poiner to logical file name.
//-----------------------------------------------------------------------------

void            AddLfn(const char * lfn);

//-----------------------------------------------------------------------------
//! Add protocol to the list of available protocols.
//!
//! @param  protname     Poiner to protocol name ending with a colon
//-----------------------------------------------------------------------------

void            AddProtocol(const char * protname);

//-----------------------------------------------------------------------------
//! Obtain the next digest that can be used to validate the file.
//!
//! @param  hval     Place to put the pointer to the hash value in ASCII
//!                  encoded hex,
//! @param  xrdname  When true the corresponding name expected by XRootD is
//!                  returned
//!
//! @return Pointer to the hash name. The name and value are valid until this
//!         object is deleted. If no more hashes exist, a nil pointer is
//!         returned. A subsequent call will start at the front of the list.
//-----------------------------------------------------------------------------

const char     *GetDigest(const char *&hval, bool xrdname=true);

//-----------------------------------------------------------------------------
//! Obtain the logical file name associated with this file.
//!
//! @return Pointer to the lfn. The lfn is valid until this object is deleted.
//!         A nil pointer indicates that no lfn has been specified.
//-----------------------------------------------------------------------------

const char     *GetLfn() {return fLfn;}

//-----------------------------------------------------------------------------
//! Obtain the target file name.
//!
//! @return Pointer to the target file name. The target filename is valid until this object is deleted.
//-----------------------------------------------------------------------------

const char     *GetTargetName() {return fTargetName;}

//-----------------------------------------------------------------------------
//! Get file size.
//!
//! @return The size of the file. If it is negative the size has not been set.
//-----------------------------------------------------------------------------

long long       GetSize() {return fSize;}

//-----------------------------------------------------------------------------
//! Obtain the next url for this file.
//!
//! @param  cntry    If not nil, the null terminated country code is placed in
//!                  the buffer which  must be atleast three characters in size.
//!
//! @param  prty     If not nil, the url's priority is placed in the int
//!                  pointed to by this parameter.
//!
//! @return Pointer to the url. The url is valid until this object is deleted.
//!         If no more urls exist, a nil pointer is returned. A subsequent call
//!         will start at the front of teh list.
//-----------------------------------------------------------------------------

const char     *GetUrl(char *cntry=0, int *prty=0);

//-----------------------------------------------------------------------------
//! Check if  protocol is in he list of protocols. This does not indicate that
//! an actual url for the protocol was added to this object.
//!
//! @param  protname     Poiner to protocol name ending with a colon
//!
//! @return true if the protocol was encountered and false otherwise.
//-----------------------------------------------------------------------------

bool            HasProtocol(const char * protname);

//-----------------------------------------------------------------------------
//! Set file size.
//!
//! @param  fsz      Supposed size of the file.
//-----------------------------------------------------------------------------

void            SetSize(long long fsz) {fSize = fsz;}

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  lfn      An optional logical file name associated with this file.
//-----------------------------------------------------------------------------

                XrdOucFileInfo(const char *lfn=0)
                              : nextFile(0), fHash(0), fHashNext(0),
                                             fUrl(0),  fUrlNext(0), fTargetName(0), fSize(-1)
                              {if (lfn) fLfn = strdup(lfn);
                                  else  fLfn = 0;
                              }

//-----------------------------------------------------------------------------
//! Destructor.
//-----------------------------------------------------------------------------

               ~XrdOucFileInfo();

//-----------------------------------------------------------------------------
//! Link field to simply miltiple file processing
//-----------------------------------------------------------------------------

XrdOucFileInfo *nextFile;

private:

XrdOucFIHash  *fHash;
XrdOucFIHash  *fHashNext;
XrdOucFIUrl   *fUrl;
XrdOucFIUrl   *fUrlNext;
char          *fLfn;
char          *fTargetName;
long long      fSize;
std::string    protList;
};
#endif

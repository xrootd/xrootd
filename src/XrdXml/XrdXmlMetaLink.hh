#ifndef __XRDXMLMETALINK_HH__
#define __XRDXMLMETALINK_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d X m l M e t a L i n k . h h                      */
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

#include <cstdlib>
#include <cstring>

#include "XrdOuc/XrdOucFileInfo.hh"
#include "XrdXml/XrdXmlReader.hh"
  
//-----------------------------------------------------------------------------
//! The XrdXmlMetaLink object provides a uniform interface to convert metalink
//! XML specifications to one or more XrdOucFileInfo objects. This object does
//! not do a rigorous syntactic check of the metalink specification.
//! Specifications that technically violate RFC 5854 (v4 metalinks) or the
//! metalink.org v3 metalinks may be accepted and yield valid information.
//-----------------------------------------------------------------------------

class XrdXmlMetaLink
{
public:

//-----------------------------------------------------------------------------
//! Convert an XML metalink specification to a file info object. Only the first
//! file entry is converted (see ConvertAll()).
//!
//! @param  fbuff    Pointer to the filepath that contains the metalink
//!                  specification when blen is 0. Otherwise, fbuff points to a
//!                  memory buffer of length blen containing the specification.
//!
//! @param  blen     Length of the buffer. When <=0, the first argument is a
//!                  file path. Otherwise, it is a memory buffer of length blen
//!                  whose contents are written into a file in /tmp, converted,
//!                  and then deleted.
//!
//! @return Pointer to the corresponding file info object upon success.
//!         Otherwise, a null pointer is returned indicating that the metalink
//!         specification was invalid or had no required protocols. Use the
//!         GetStatus() method to obtain the description of the problem.
//-----------------------------------------------------------------------------

XrdOucFileInfo  *Convert(const char *fbuff, int blen=0);

//-----------------------------------------------------------------------------
//! Convert an XML metalink specification to a file info object. All file
//! entries are converted.
//!
//! @param  fbuff    Pointer to the filepath that contains the metalink
//!                  specification when blen is 0. Otherwise, fbuff points to a
//!                  memory buffer of length blen containing the specification.
//!
//! @param  count    Place where the number of array elements is returned.
//!
//! @param  blen     Length of the buffer. When <=0, the first argument is a
//!                  file path. Otherwise, it is a memory buffer of length blen
//!                  whose contents are written into a file in /tmp, converted,
//!                  and then deleted.
//!
//! @return Pointer to the array of corresponding fil info objects upon success. Otherwise,
//!         Otherwise, a null pointer is returned indicating that the metalink
//!         specification was invalid or had no required protocols. Use the
//!         GetStatus() method to obtain the description of the problem.
//!         Be aware that you must first delete each file info object before
//!         deleting the array. You can do this via DeleteAll().
//-----------------------------------------------------------------------------

XrdOucFileInfo **ConvertAll(const char *fbuff, int &count, int blen=0);

//-----------------------------------------------------------------------------
//! Delete a vector of file info objects and the vector itself as well.
//!
//! @param  vecp     Pointer to the array.
//! @param  vecn     Number of elements in the vector.
//-----------------------------------------------------------------------------

static void     DeleteAll(XrdOucFileInfo **vecp, int vecn);

//-----------------------------------------------------------------------------
//! Obtain ending status of previous conversion.
//!
//! @param  ecode    Place to return the error code, if any.
//!
//! @return Pointer to the error text describing the error. The string becomes
//!         invalid if Convert() is called or the object is deleted. If no
//!         error was encountered, a null string is returned with ecode == 0.
//-----------------------------------------------------------------------------

const char      *GetStatus(int &ecode) {ecode = eCode; return eText;}

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  protos   Pointer to the list of desired protocols. Each protocol ends
//!                  with a colon. They are specified without embedded spaces.
//!                  Only urls using one of the listed protocols is returned.
//!                  A nil pointer returns all urls regardless of the protocol.
//! @param  rdprot   The protocol to be used when constructing the global file
//!                  entry. If nil, the first protocol in protos is used. If
//!                  nil, a global file is not constructed.
//! @param  rdhost   The "<host>[<port>]" to use when constructing the global
//!                  file. A global file entry is constructed only if rdhost
//!                  is specified and a protocol is available, and a global
//!                  file element exists in the xml file.
//!
//! @param  encode   Specifies the xml encoding. Currently, only UTF-8 is
//!                  is supported and is signified by a nil pointer.
//-----------------------------------------------------------------------------

                XrdXmlMetaLink(const char *protos="root:xroot:",
                               const char *rdprot="xroot:",
                               const char *rdhost=0,
                               const char *encode=0
                              ) : reader(0),
                                  fileList(0), lastFile(0), currFile(0),
                                  prots(protos   ? strdup(protos) : 0),
                                  encType(encode ? strdup(encode) : 0),
                                  rdProt(rdprot), rdHost(rdhost),
                                  fileCnt(0), eCode(0),
                                  doAll(false), noUrl(true)
                               {*eText = 0; *tmpFn = 0;}

//-----------------------------------------------------------------------------
//! Destructor.
//-----------------------------------------------------------------------------

               ~XrdXmlMetaLink() {if (prots)   free(prots);
                                  if (encType) free(encType);
                                 }

private:
bool            GetFile(const char *scope);
bool            GetFileInfo(const char *scope);
bool            GetGLfn();
bool            GetHash();
void            GetRdrError(const char *why);
bool            GetSize();
bool            GetUrl();
void            GetName();
bool            PutFile(const char *buff, int blen);
bool            UrlOK(char *url);

XrdXmlReader   *reader;
XrdOucFileInfo *fileList;
XrdOucFileInfo *lastFile;
XrdOucFileInfo *currFile;
char           *prots;
char           *encType;
const char     *rdProt;
const char     *rdHost;
int             fileCnt;
int             eCode;
bool            doAll;
bool            noUrl;
char            tmpFn[64];
char            eText[256];
};
#endif

#ifndef __ACC_AUTHDB__
#define __ACC_AUTHDB__
/******************************************************************************/
/*                                                                            */
/*                       X r d A c c A u t h D B . h h                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysError.hh"
  
// This class is provided for obtaining capability information from some source.
// Derive a class to provide an actual source for the information. The
// interface is similar to the set/get/endpwent enumeration interface:

// setDBpath() is used to establish the location of the database.

// Open()     establishes the start of the database operation. It also obtains
//            an exclusive mutex to be mt-safe. True is returned upon success.

// getRec()   get the next database record. It returns the record type as well
//            as a pointer to the record name. False is returned at the end
//            of the database.

// getPP()    gets the next path-priv or template name. It returns a pointer
//            to each one. True is returned until end-of-record.

// Close()    terminates database processing and releases the associated lock.
//            It also return FALSE if any errors occured during processing.

// Changed()  Returns 1 id the current authorization file has changed since
//            the last time it was opened.

  
/******************************************************************************/
/*                 D a t a b a s e   R e c o r d   T y p e s                  */
/******************************************************************************/
  
// The following are the 1-letter id types that we support
//
// g -> unix group name
// h -> host name
// n -> NIS netgroup name
// s -> set name
// t -> template name
// u -> user name

// The syntax for each database record is:

// <RecType> <recname> {<tname>|<path> <priv>} [{<tname|<path> <priv>}] [...]

// = <idname> <id:> <idval> [<id:> <idval> [....]]

// Continuation records are signified by an ending backslash (\). Blank records
// and comments (i.e., lines with the first non-blank being a pound sign) are
// allowed. Word separators may be spaces or tabs.

/******************************************************************************/
/*                    X r d A c c A u t h D B   C l a s s                     */
/******************************************************************************/
  
class XrdAccAuthDB
{
public:

virtual int   Open(XrdSysError &eroute, const char *path=0) = 0;

virtual char  getRec(char **recname) = 0;

virtual char  getID(char **id) = 0;

virtual int   getPP(char **path, char **priv, bool &istmplt) = 0;

virtual int   Close() = 0;

virtual int   Changed(const char *path=0) = 0;

              XrdAccAuthDB() {}
virtual      ~XrdAccAuthDB() {}

};

/******************************************************************************/
/*                   X r d A c c X u t h D B _ O b j e c t                    */
/******************************************************************************/
  
extern XrdAccAuthDB *XrdAccAuthDBObject(XrdSysError *erp);

#endif

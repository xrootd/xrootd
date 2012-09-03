#ifndef __SYS_DIR_H__
#define __SYS_DIR_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d S y s D i r . h h                                */
/*                                                                            */
/* (c) 2006 G. Ganis (CERN)                                                   */
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
/*     All Rights Reserved. See XrdInfo.cc for complete License Terms         */
/******************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdSysDir                                                            //
//                                                                      //
// Author: G. Ganis, CERN, 2006                                         //
//                                                                      //
// API for handling directories                                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#if !defined(WINDOWS)
#  include <sys/types.h>
#else
#  define uid_t unsigned int
#  define gid_t unsigned int
#endif

class XrdSysDir
{
 public:
   XrdSysDir(const char *path);
   virtual ~XrdSysDir();

   bool  isValid() { return (dhandle ? 1 : 0); }
   int   lastError() { return lasterr; }
   char *nextEntry();

 private:
   void  *dhandle;  // Directory handle
   int    lasterr;  // Error occured at last operation
};
#endif

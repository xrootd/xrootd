#ifndef XRDCPWORKLST_HH
#define XRDCPWORKLST_HH
/******************************************************************************/
/*                                                                            */
/*                     X r d C p W o r k L s t . h h                          */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// A class implementing a list of cp to do for XrdCp                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClient.hh"
#include <stdint.h>

class XrdSysDir;
const char *ServerError(XrdClient *cli);
void PrintLastServerError(XrdClient *cli);
bool PedanticOpen4Write(XrdClient *cli, kXR_unt16 mode, kXR_unt16 options);

//------------------------------------------------------------------------------
// Check if the opaque data provides the file size information and add it
// if needed
//------------------------------------------------------------------------------
XrdOucString AddSizeHint( const char *dst, off_t size );

class XrdCpWorkLst {

   vecString fWorkList;
   uint64_t pSourceSize;  // set if the source URL refers to a file
   int srcPathLen;
   int fWorkIt;

   XrdClientAdmin *xrda_src, *xrda_dst;

   XrdOucString fSrc, fDest;
   bool fDestIsDir, fSrcIsDir;

 public:
   
   XrdCpWorkLst();
   ~XrdCpWorkLst();


   // Sets the source path for the file copy
   int SetSrc(XrdClient **srccli, XrdOucString url,
	      XrdOucString urlopaquedata, bool do_recurse, int newCP=0);

   // Sets the destination of the file copy
   int SetDest(XrdClient **xrddest, const char *url,
	       const char *urlopaquedata,
	       kXR_unt16 xrdopenflags, int newCP=0);

   inline void GetDest(XrdOucString &dest, bool& isdir) {
      dest = fDest;
      isdir = fDestIsDir;
   }

   inline void GetSrc(XrdOucString &src, bool& isdir) {
      src = fSrc;
      isdir = fSrcIsDir;
   }


   // Actually builds the worklist
   int BuildWorkList_xrd(XrdOucString url, XrdOucString opaquedata);
   int BuildWorkList_loc(XrdSysDir *dir, XrdOucString pat);

   bool GetCpJob(XrdOucString &src, XrdOucString &dest);
   
};
#endif

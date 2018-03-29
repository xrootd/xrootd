#ifndef __XRDCMSRRDATA_H__
#define __XRDCMSRRDATA_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s R R D a t a . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>

#include "XProtocol/YProtocol.hh"

class XrdCmsRLData
{
public:

       char          *theAuth;
       char          *theSID;
       char          *thePaths;
       int            totLen;

//     XrdCmsRLData() {}  Lack of constructor makes this a POD type
//    ~XrdCmsRLData() {}  Lack of destructor  makes this a POD type
};


class XrdCmsRRData
{
public:
XrdCms::CmsRRHdr       Request;     // all
        char          *Path;        // all -prepcan
        char          *Opaque;      // all -prepcan
        char          *Path2;       // mv
        char          *Opaque2;     // mv
        char          *Avoid;       // locate, select
        char          *Reqid;       // prepadd, prepcan
        char          *Notify;      // prepadd
        char          *Prty;        // prepadd
        char          *Mode;        // chmod, mkdir, mkpath, prepadd
        char          *Ident;       // all
        unsigned int   Opts;        // locate, select
                 int   PathLen;     // locate, prepadd, select (inc null byte)
        unsigned int   dskFree;     // avail, load
union  {unsigned int   dskUtil;     // avail
                 int   waitVal;
       };
        char          *Buff;        // Buffer underlying the pointers
        int            Blen;        // Length of buffer
        int            Dlen;        // Length of data in the buffer
        int            Routing;     // Routing options

enum ArgName
{    Arg_Null=0,   Arg_AToken,    Arg_Avoid,     Arg_Datlen,
     Arg_Ident,    Arg_Info,      Arg_Mode,      Arg_Notify,
     Arg_Opaque2,  Arg_Opaque,    Arg_Opts,      Arg_Path,
     Arg_Path2,    Arg_Port,      Arg_Prty,      Arg_Reqid,
     Arg_dskFree,  Arg_dskUtil,   Arg_theLoad,   Arg_SID,
     Arg_dskTot,   Arg_dskMinf,   Arg_CGI,       Arg_Ilist,

     Arg_Count     // Always the last item which equals the number of elements
};

static XrdCmsRRData *Objectify(XrdCmsRRData *op=0);

       int           getBuff(size_t bsz);

//      XrdCmsRRData() {}  Lack of constructor makes this a POD type
//     ~XrdCmsRRData() {}  Lack of destructor  makes this a POD type

XrdCmsRRData *Next;   // POD types canot have private members so virtual private
};
#endif

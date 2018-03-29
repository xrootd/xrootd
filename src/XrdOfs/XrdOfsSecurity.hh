#ifndef ___XrdOfsSECURITY_H___
#define ___XrdOfsSECURITY_H___
/******************************************************************************/
/*                                                                            */
/*                     X r d O f s S e c u r i t y . h h                      */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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

#include "XrdAcc/XrdAccAuthorize.hh"

#define AUTHORIZE(usr, env, optype, action, pathp, edata) \
    if (usr && XrdOfsFS->Authorization \
    &&  !XrdOfsFS->Authorization->Access(usr, pathp, optype, env)) \
       {XrdOfsFS->Emsg(epname, edata, EACCES, action, pathp); return SFS_ERROR;}

#define AUTHORIZE2(usr,edata,opt1,act1,path1,env1,opt2,act2,path2,env2) \
       {AUTHORIZE(usr, env1, opt1, act1, path1, edata); \
        AUTHORIZE(usr, env2, opt2, act2, path2, edata); \
       }

#define OOIDENTENV(usr, env) \
    if (usr) {if (usr->name) env.Put(SEC_USER, usr->name); \
              if (usr->host) env.Put(SEC_HOST, usr->host);}
#endif

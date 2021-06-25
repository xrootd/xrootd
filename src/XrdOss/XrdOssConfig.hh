#ifndef _XRDOSS_CONFIG_H
#define _XRDOSS_CONFIG_H
/******************************************************************************/
/*                                                                            */
/*                       X r d O s s C o n f i g . h h                        */
/*                                                                            */
/* (C) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#define  XRDOSS_VERSION "2.0.0"

/* Constant to indicate all went well.
*/
#ifndef XrdOssOK
#define XrdOssOK 0
#endif

// Flags set in OptFlags
//
#define XrdOss_USRPRTY   0x00000001
#define XrdOss_CacheFS   0x00000002

// Small structure to hold dual paths
//
struct  OssDPath
       {OssDPath *Next;
        char     *Path1;
        char     *Path2;
        OssDPath(OssDPath *dP,char *p1,char *p2) : Next(dP),Path1(p1),Path2(p2) {}
       };

class   XrdOucString;

struct  OssSpaceConfig
       {const XrdOucString& sName;
        const XrdOucString& sPath;
        const XrdOucString& mName;
        bool          isXA;
        bool          noFail;
        bool          chkMnt;
        OssSpaceConfig(XrdOucString& sn, XrdOucString& sp, XrdOucString& mn)
                      : sName(sn), sPath(sp), mName(mn),
                        isXA(true), noFail(false), chkMnt(false)
                      {}
       };

#endif

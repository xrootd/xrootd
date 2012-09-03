#ifndef __ACC_PRIVS__
#define __ACC_PRIVS__
/******************************************************************************/
/*                                                                            */
/*                        X r d A c c P r i v s . h h                         */
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

/******************************************************************************/
/*                           X r d A c c P r i v s                            */
/******************************************************************************/
  
// Recognized privileges
//
enum XrdAccPrivs {XrdAccPriv_All    = 0x07f,
                  XrdAccPriv_Chmod  = 0x063,  // Insert + Open r/w + Delete
                  XrdAccPriv_Chown  = 0x063,  // Insert + Open r/w + Delete
                  XrdAccPriv_Create = 0x062,  // Insert + Open r/w
                  XrdAccPriv_Delete = 0x001,
                  XrdAccPriv_Insert = 0x002,
                  XrdAccPriv_Lock   = 0x004,
                  XrdAccPriv_Mkdir  = 0x002,  // Insert
                  XrdAccPriv_Lookup = 0x008,
                  XrdAccPriv_Rename = 0x010,
                  XrdAccPriv_Read   = 0x020,
                  XrdAccPriv_Readdir= 0x020,
                  XrdAccPriv_Write  = 0x040,
                  XrdAccPriv_Update = 0x060,
                  XrdAccPriv_None   = 0x000
                 };
  
/******************************************************************************/
/*                        X r d A c c P r i v S p e c                         */
/******************************************************************************/
  
// The following are the 1-letter privileges that we support.
//
enum XrdAccPrivSpec {   All_Priv = 'a',
                     Delete_Priv = 'd',
                     Insert_Priv = 'i',
                       Lock_Priv = 'k',
                     Lookup_Priv = 'l',
                     Rename_Priv = 'n',
                       Read_Priv = 'r',
                      Write_Priv = 'w',
                        Neg_Priv = '-'
                    };

/******************************************************************************/
/*                        X r d A c c P r i v C a p s                         */
/******************************************************************************/

struct XrdAccPrivCaps {XrdAccPrivs pprivs;     // Positive privileges
                       XrdAccPrivs nprivs;     // Negative privileges

                       XrdAccPrivCaps() {pprivs = XrdAccPriv_None;
                                         nprivs = XrdAccPriv_None;
                                        }
                      ~XrdAccPrivCaps() {}

                      };
#endif

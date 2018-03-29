#ifndef __XRDPOSIXADMIN_HH__
#define __XRDPOSIXADMIN_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x A d m i n . h h                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

/******************************************************************************/
/*                         X r d P o s i x A d m i n                          */
/******************************************************************************/
  
class XrdPosixAdmin
{
public:

XrdCl::URL        Url;
XrdCl::FileSystem Xrd;

bool           isOK() {if (Url.IsValid()) return true;
                       errno = EINVAL;    return false;
                      }

XrdCl::URL    *FanOut(int &num);

int            Query(XrdCl::QueryCode::Code reqCode, void *buff, int bsz);

bool           Stat(mode_t *flags=0, time_t *mtime=0,
                    size_t *size=0,  ino_t  *id=0, dev_t *rdv=0);

      XrdPosixAdmin(const char *path)
                      : Url((std::string)path), Xrd(Url) {}
     ~XrdPosixAdmin() {}
};
#endif

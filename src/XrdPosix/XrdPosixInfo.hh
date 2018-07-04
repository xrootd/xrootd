#ifndef __XRDPOSIXINFO_HH__
#define __XRDPOSIXINFO_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d P o s i x I n f o . h h                        */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <sys/param.h>

class XrdPosixCallBack;

struct XrdPosixInfo
{
XrdPosixCallBack *cbP;
int               fileFD;
bool              ffReady;
char              cacheURL[7];
char              cachePath[MAXPATHLEN];

                  XrdPosixInfo(XrdPosixCallBack *cbp=0)
                              : cbP(cbp), fileFD(-1), ffReady(false)
                              {memcpy(cacheURL, "file://", 7);
                               *cachePath = 0;
                              }
                 ~XrdPosixInfo() {}
};
#endif

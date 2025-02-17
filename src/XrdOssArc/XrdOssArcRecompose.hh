#ifndef _XRDOSSARCRECOMPOSE_H
#define _XRDOSSARCRECOMPOSE_H
/******************************************************************************/
/*                                                                            */
/*                 X r d O s s A r c R e c o m p o s e . h h                  */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>

class XrdOssArcRecompose
{

public:

       char* arcPath = 0; // Full path to file within this RSE
       char* arcDir  = 0; // Directory where file should exist (w/ prefix & sep)
const  char* arcFile = 0; // File path, points into arcPath
       char* arcDSN  = 0; // The dataset name

       bool        Compose(char* buff, int bsz);

static bool        isArcFile(const char *path);

             XrdOssArcRecompose(const char *path, int& retc, bool isW=true);

            ~XrdOssArcRecompose();

XrdOssArcRecompose& operator=(XrdOssArcRecompose&) = delete;

XrdOssArcRecompose& operator=(XrdOssArcRecompose&& rhs)
                             {if (this != &rhs)
                                 {arcPath = rhs.arcPath; rhs.arcPath = 0;
                                  arcDir  = rhs.arcDir;  rhs.arcDir  = 0;
                                  arcFile = rhs.arcFile; rhs.arcFile = 0;
                                  arcDSN  = rhs.arcDSN;  rhs.arcDSN  = 0;
                                 }
                              return *this;
                             }

private:
static int   minLenDSN;
static int   minLenFN;
};
#endif

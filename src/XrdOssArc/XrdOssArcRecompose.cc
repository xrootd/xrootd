/******************************************************************************/
/*                                                                            */
/*                 X r d O s s A r c R e c o m p o s e . c c                  */
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcRecompose.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdOssArcConfig Config;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
  
int   XrdOssArcRecompose::minLenDSN = 4;
int   XrdOssArcRecompose::minLenFN  = 4;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOssArcRecompose::XrdOssArcRecompose(const char *path, int& rc, bool isW)
{
   TraceInfo("Recompose",0);

// By conventioon, a special charcater is used to separate the dataset name
// from the data ID (the file path).
//

// Do not include the prefix here
//
   if (strncmp(Config.arcvPathLFN, path, Config.arcvPathLEN))
      {rc = EDOM;
       return;
      }

// Make sure the path does not end with the archive file suffix, for writing
//
   if (isW && isArcFile(path))
      {rc = EDOM;
       return;
      }

// By convention the separator character establishes the end of the datset
// name and its use as a directory prefix path. So, we expect that this will
// be of the form  "/<pfx>/<dsn><sep>/<filepath>". We must verify that it is.
// We are optimistic that this will be the case.

// Duplicate the path and file dsn separator
//
   char *sepP, *dsnP;
   arcPath = strdup(path);   
   if (!(sepP = index(arcPath, Config.mySep)) || *(sepP+1) != '/')
      {rc = EINVAL;
       return;
      }

// Make sure the dsn and a path exist here
//
   *(sepP+1) = 0;
   dsnP = arcPath+Config.arcvPathLEN;
   if ((int)strlen(dsnP) < minLenDSN || (int)strlen(sepP+2) < minLenFN)
      {rc = EINVAL;
       return;
      }

// Extract the dataset name, it does not include the separator character
// Example: Directory: /archive/mydsn# Datasetname: mydsn
//
    *sepP   = 0;
    arcDSN = strdup(dsnP);

// Extract out the directory name (see the convention comment). Note that
// we have checked that we have "<sep>/" which is why the code here works.
//
   char tmpC[4], seqC[4];
   memcpy(tmpC, sepP, sizeof(tmpC));
   *sepP      = '/';
   *(sepP+1)  = Config.mySep;
   *(sepP+2)  = '/';
   *(sepP+3)  = 0;
   memcpy(seqC, sepP, sizeof(seqC));

   arcDir     = strdup(arcPath); // <dataset_name>/<sep>/

   memcpy(sepP, tmpC, sizeof(tmpC));
   arcFile    = sepP+2;
   rc = 0;

// The arcDir is the directory path we will use to store the dataset files
// as well as certain control information to aid in recovery from a crash.
// This directory must be atomically unique to avoid race conditions when
// dataset paths partially overlap; which is allowed. However, every *full*
// dataset path is unique so we can avoid overlaps by simply flattening the
// dataset path and using it as the actual directory.
//
   sepP = arcDir + Config.arcvPathLEN;

   while(*sepP && memcmp(sepP, seqC, sizeof(seqC)))
        {if (*sepP == '/') *sepP = Config.mySep;
         sepP++;
        }

// Do some debugging
//
   DEBUG("arcPath="<<arcPath<<"\narcDir="<<arcDir<<"\narcFile="<<arcFile
                            <<"\narcDSN="<<arcDSN); 
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOssArcRecompose::~XrdOssArcRecompose()
{
   if (arcPath) free(arcPath);
   if (arcDir)  free(arcDir);
   if (arcDSN)  free(arcDSN);
}

/******************************************************************************/
/*                               C o m p o s e                                */
/******************************************************************************/

char* XrdOssArcRecompose::Compose(char* buff, int bsz)
{            
// Construct new path
//
   int n = snprintf(buff, bsz, "%s%s", arcDir, arcFile);
   return (n < bsz ? buff : 0); 
}
  
/******************************************************************************/
/*                             i s A r c F i l e                              */
/******************************************************************************/
  
bool XrdOssArcRecompose::isArcFile(const char *path)
{
   int n = strlen(path);

// Is it's too short, it cannot end with ".zip" (or what it the suffix is)
//
   if (n <= Config.arfSfxLen) return false;

// Verify the ending
//
   return !strncmp(Config.arfSfx, path+n-Config.arfSfxLen, Config.arfSfxLen);
}

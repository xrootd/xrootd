/******************************************************************************/
/*                                                                            */
/*                          X r d S s i D i r . c c                           */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <string.h>

#include "XrdOuc/XrdOucPList.hh"
#include "XrdSsi/XrdSsiDir.hh"
#include "XrdSsi/XrdSsiUtils.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSfsFileSystem *theFS;
extern XrdOucPListAnchor FSPath;
extern bool              fsChk;
};

using namespace XrdSsi;

/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdSsiDir::open(const char              *dir_path, // In
                    const XrdSecEntity      *client,   // In
                    const char              *info)      // In
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.
            client    - Authentication credentials, if any.
            info      - Opaque information to be used as seen fit.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR.
*/
{
   static const char *epname = "opendir";
   int eNum;

// Verify that this object is not already associated with an open file
//
   if (dirP)
      return XrdSsiUtils::Emsg(epname,EADDRINUSE,"open directory",dir_path,error);

// Open a regular file if this is wanted
//
   if (fsChk)
      {if (!FSPath.Find(dir_path))
          {if (!(dirP = theFS->newDir((char *)tident, error.getErrMid())))
              return XrdSsiUtils::Emsg(epname, ENOMEM, epname, dir_path, error);
           error.Reset(); dirP->error = error;
           if ((eNum = dirP->open(dir_path, client, info)))
              {error = dirP->error;
               delete dirP; dirP = 0;
              } else return SFS_OK;
          } else error.setErrInfo(ENOTSUP, "Directory operations not "
                                           "not supported on given path.");
       } else error.setErrInfo(ENOTSUP, "Directory operations not supported.");

// All done
//
   return SFS_ERROR;
}

/******************************************************************************/
/*                             n e x t E n t r y                              */
/******************************************************************************/

const char *XrdSsiDir::nextEntry()
/*
  Function: Read the next directory entry.

  Input:    n/a

  Output:   Upon success, returns the contents of the next directory entry as
            a null terminated string. Returns a null pointer upon EOF or an
            error. To differentiate the two cases, getErrorInfo will return
            0 upon EOF and an actual error code (i.e., not 0) on error.
*/
{
   const char *epname = "readdir";
   const char *dent;

// Check if this directory is actually open
//
   if (!dirP) {XrdSsiUtils::Emsg(epname, EBADF, epname, "???", error);
               return 0;
              }

// Read the next directory entry
//
   dent = dirP->nextEntry();
   if (!dent) error = dirP->error;

// Return the actual entry
//
   return dent;
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/
  
int XrdSsiDir::close()
/*
  Function: Close the directory object.

  Input:    n/a

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   const char *epname = "closedir";
   int retc;

// Check if this directory is actually open
//
   if (!dirP) return XrdSsiUtils::Emsg(epname, EBADF, epname, "???", error);

// Close this directory
//
    if ((retc = dirP->close())) error = dirP->error;

// All done
//
   delete dirP;
   dirP = 0;
   return retc;
}

/******************************************************************************/
/*                              a u t o S t a t                               */
/******************************************************************************/

int XrdSsiDir::autoStat(struct stat *buf)
/*
  Function: Set stat buffer to automaticaly return stat information

  Input:    Pointer to stat buffer which will be filled in on each
            nextEntry() and represent stat information for that entry.

  Output:   Upon success, returns zero. Upon error returns SFS_ERROR and sets
            the error object to contain the reason.
*/
{
   const char *epname = "autoStat";
   int retc;

// Check if this directory is actually open
//
   if (!dirP) return XrdSsiUtils::Emsg(epname, EBADF, epname, "???", error);

// Do the autostat
//
   if ((retc = dirP->autoStat(buf))) error = dirP->error;

// All done
//
   return retc;
}

/******************************************************************************/
/*                                 F N a m e                                  */
/******************************************************************************/
  
const char *XrdSsiDir::FName()
{
   const char *epname = "fname";

// Check if this directory is actually open
//
   if (!dirP) return dirP->FName();
   XrdSsiUtils::Emsg(epname, EBADF, epname, "???", error);
   return "";
}

/******************************************************************************/
/*                                                                            */
/*                         X r d O u c S x e q . c c                          */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <cerrno>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucSxeq.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucSxeq::XrdOucSxeq(int sOpts, const char *path)
{
   static const int AMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   lokFN = strdup(path);
   lokUL = 0;

// Open the file, creating it
//
   if ((lokFD = open(lokFN, O_CREAT|O_RDWR, AMode)) < 0) lokRC = errno;
      else {lokRC = 0;
            if (sOpts) Serialize(sOpts);
           }
}

/******************************************************************************/
  
XrdOucSxeq::XrdOucSxeq(const char *sfx1, const char *sfx2, const char *Dir)
{
   static const int AMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   char pbuff[MAXPATHLEN+1], *pP;

// Construct the lock file name
//
   strcpy(pbuff, Dir);
   pP = pbuff + strlen(Dir);
   if (*sfx1 != '/' && *(pP-1) != '/') *pP++ = '/';
   strcpy(pP, sfx1);
   if (sfx2) strcpy(pP+strlen(sfx1), sfx2);
   lokFN = strdup(pbuff);
   lokUL = 0;

// Open the file, creating it
//
   if ((lokFD = open(lokFN, O_CREAT|O_RDWR, AMode)) < 0) lokRC = errno;
      else lokRC = 0;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucSxeq::~XrdOucSxeq()
{

// Check if we should unlink this file we need to do so while it's locked)
//
   if (lokFD >= 0 && lokUL) unlink(lokFN);

// Close the file and free th file name
//
   if (lokFD >= 0) close(lokFD);
   free(lokFN);
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
int XrdOucSxeq::Release()
{
   FLOCK_t lock_args;
   int rc;

// If the file is not open, return failure
//
   if (lokFD < 0) return 0;

// Establish locking options
//
   bzero(&lock_args, sizeof(lock_args));
   lock_args.l_type = F_UNLCK;

// Now perform the action
//
   do {rc = fcntl(lokFD, F_SETLKW, &lock_args);}
      while(rc < 0 && errno == EINTR);

// Determine result
//
   if (rc < 0) {lokRC = errno; return 0;}

// We succeeded, unlink is not possible now
//
   lokUL = 0;
   lokRC = 0;
   return 1;
}
/******************************************************************************/
  
int XrdOucSxeq::Release(int fileD)
{
   FLOCK_t lock_args;
   int rc;

// If the file is not open, return failure
//
   if (fileD < 0) return EBADF;

// Establish locking options
//
   bzero(&lock_args, sizeof(lock_args));
   lock_args.l_type = F_UNLCK;

// Now perform the action
//
   do {rc = fcntl(fileD, F_SETLKW, &lock_args);}
      while(rc < 0 && errno == EINTR);

// Return result
//
   return (rc ? errno : 0);
}
  
/******************************************************************************/
/*                             S e r i a l i z e                              */
/******************************************************************************/
  
int XrdOucSxeq::Serialize(int Opts)
{
   FLOCK_t lock_args;
   int Act, rc;

// If the file is not open, return failure
//
   if (lokFD < 0) return 0;

// Establish lock flags
//

// Establish locking options
//
   bzero(&lock_args, sizeof(lock_args));
   lock_args.l_type = (Opts & Share ? F_RDLCK : F_WRLCK);
   Act = (Opts & noWait ? F_SETLK : F_SETLKW);

// Now perform the action
//
   do {rc = fcntl(lokFD, Act, &lock_args);} while(rc < 0 && errno == EINTR);

// Determine result
//
   if (rc < 0) {lokRC = errno; return 0;}

// We succeeded check if an unlink is possible
//
   if (Opts & Unlink && !(Opts & Share)) lokUL = 1;
   lokRC = 0;
   return 1;
}

/******************************************************************************/

int XrdOucSxeq::Serialize(int fileD, int opts)
{
    FLOCK_t lock_args;

// Make sure we have a lock outstanding
//
    if (fileD < 0) return EBADF;

// Establish locking options
//
    bzero(&lock_args, sizeof(lock_args));
    if (opts & Share) lock_args.l_type = F_RDLCK;
       else           lock_args.l_type = F_WRLCK;

// Perform action.
//
    if (fcntl(fileD, (opts & noWait ? F_SETLK : F_SETLKW), &lock_args))
       return errno;
    return 0;
}

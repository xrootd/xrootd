#ifndef __XRDOSS_ETEXT__
#define __XRDOSS_ETEXT__
/******************************************************************************/
/*                                                                            */
/*                        X r d O s s E T e x t . h h                         */
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

#define XRDOSS_EBASE 8001
  
#define XRDOSS_E8001 8001
#define XRDOSS_E8002 8002
#define XRDOSS_E8003 8003
#define XRDOSS_E8004 8004
#define XRDOSS_E8005 8005
#define XRDOSS_E8006 8006
#define XRDOSS_E8007 8007
#define XRDOSS_E8008 8008
#define XRDOSS_E8009 8009
#define XRDOSS_E8010 8010
#define XRDOSS_E8011 8011
#define XRDOSS_E8012 8012
#define XRDOSS_E8013 8013
#define XRDOSS_E8014 8014
#define XRDOSS_E8015 8015
//      XRDOSS_E8016 8016
#define XRDOSS_E8017 8017
#define XRDOSS_E8018 8018
#define XRDOSS_E8019 8019
#define XRDOSS_E8020 8020
#define XRDOSS_E8021 8021
#define XRDOSS_E8022 8022
#define XRDOSS_E8023 8023
#define XRDOSS_E8024 8024
#define XRDOSS_E8025 8025
#define XRDOSS_E8026 8026
#define XRDOSS_E8027 8027
#define XRDOSS_E8028 8028

#define XRDOSS_ELAST 8028

#define XRDOSS_N8001 EBUSY   // directory object in use
#define XRDOSS_N8002 EBADF   // directory object not open
#define XRDOSS_N8003 EBUSY   // file object in use
#define XRDOSS_N8004 EBADF   // file object not open
#define XRDOSS_N8005 EROFS   // file path marked read-only
#define XRDOSS_N8006 EPERM   // dynamic staging not allowed
#define XRDOSS_N8007 EFBIG   // max allowed file size exceeded
#define XRDOSS_N8008 ENOSYS  // large file support not enabled
#define XRDOSS_N8009 EIO     // dynamic staging failed
#define XRDOSS_N8010 EINVAL  // invalid staging priority
#define XRDOSS_N8011 EXDEV   // new path violates old path options
#define XRDOSS_N8012 EIO     // invalid response from mss
#define XRDOSS_N8013 E2BIG   // mss command too long
#define XRDOSS_N8014 EBUSY   // lock object in use
#define XRDOSS_N8015 EAGAIN  // unable to lock file
#define XRDOSS_N8016 EIO     // 8016
#define XRDOSS_N8017 EPERM   // unlocking an unlocked object
#define XRDOSS_N8018 EINVAL  // invalid suggested allocation size
#define XRDOSS_N8019 ENOENT  // requested space does not exist
#define XRDOSS_N8020 ENOSPC  // not enough free space
#define XRDOSS_N8021 ENOTSUP // server-side decompression disabled
#define XRDOSS_N8022 EROFS   // compressed files may not be updated
#define XRDOSS_N8023 EIO     // no response from remote storage service
#define XRDOSS_N8024 EIO     // invalid response from remote storage service
#define XRDOSS_N8025 EAGAIN  // unable to queue stage request
#define XRDOSS_N8026 EACCES  // file creation prohibited
#define XRDOSS_N8027 EINVAL  // path is not relative
#define XRDOSS_N8028 EFAULT  // dynamic cast failed

#define XRDOSS_T8001 "directory object in use (internal error)"
#define XRDOSS_T8002 "directory object not open (internal error)"
#define XRDOSS_T8003 "file object in use (internal error)"
#define XRDOSS_T8004 "file object not open (internal error)"
#define XRDOSS_T8005 "file path marked read/only"
#define XRDOSS_T8006 "dynamic staging not allowed"
#define XRDOSS_T8007 "maximum allowed file size exceeded"
#define XRDOSS_T8008 "large file support not enabled"
#define XRDOSS_T8009 "dynamic staging failed"
#define XRDOSS_T8010 "invalid staging priority"
#define XRDOSS_T8011 "new path violates old path options"
#define XRDOSS_T8012 "invalid response from mss (internal error)"
#define XRDOSS_T8013 "mss command too long (internal error)"
#define XRDOSS_T8014 "lock object in use (internal error)"
#define XRDOSS_T8015 "unable to lock file"
#define XRDOSS_T8016 "8016"
#define XRDOSS_T8017 "unlocking an unlocked object (internal error)"
#define XRDOSS_T8018 "invalid suggested allocation size"
#define XRDOSS_T8019 "requested space does not exist"
#define XRDOSS_T8020 "not enough free space to create file"
#define XRDOSS_T8021 "server-side decompression is disabled"
#define XRDOSS_T8022 "compressed files may not be open for update"
#define XRDOSS_T8023 "no response from remote storage service"
#define XRDOSS_T8024 "invalid response from remote storage service"
#define XRDOSS_T8025 "unable to queue stage request to the remote storage service"
#define XRDOSS_T8026 "file creation is prohibited"
#define XRDOSS_T8027 "path is not a relative path"
#define XRDOSS_T8028 "dynamic cast failed (internal error)"
#endif

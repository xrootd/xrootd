/******************************************************************************/
/*                                                                            */
/*                        X r d S y s S t a t x . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#ifndef XROOTD_XRDSYSSTATX_HH
#define XROOTD_XRDSYSSTATX_HH

#include <sys/stat.h>
#include <cstdint>
#include <cstring>

#ifdef __linux__
#include <sys/sysmacros.h>
typedef struct statx XrdSysStatx;
#define HAVE_STATX
#else
struct XrdSysStatx
{uint32_t stx_mask;
  struct stat statx;
};

#define STATX_BASIC_STATS 0x0000003f
#define STATX_ALL         0x0000013f
#define STATX_BTIME       0x00000100

typedef struct timespec statx_timestamp;

#endif // __linux__

class XrdSysStatxHelpers {
public:
  /**
   * Converts a stat structure into a statx structure
   * @param stat the stat structure to convert
   * @param statx the converted statx structure
   */
  static void Stat2Statx(const struct stat & stat, XrdSysStatx & statx);

  /**
   * Converts a statx structure into a stat structure
   * @param statx the statx structure to convert
   * @param stat the converted stat structure
   */
  static void Statx2Stat(const XrdSysStatx & statx, struct stat & stat);

  /**
   * Converts a statx timestamp to a stat timestamp
   * @param stx_T the statx timestamp to convert
   * @param sta_T the converted stat timestamp
   */
  static void StatxT2StatT(const statx_timestamp & stx_T, struct timespec & sta_T);

  /**
   * Converts a stat timestamp to a statx timestamp
   * @param sta_T the stat timestamp to convert
   * @param stx_T the converted statx timestamp
   */
  static void StatT2StatxT(const struct timespec & sta_T, statx_timestamp & stx_T);
};

inline void XrdSysStatxHelpers::StatxT2StatT(const statx_timestamp & stx_T, struct timespec & sta_T) {
  sta_T.tv_sec  = stx_T.tv_sec;
  sta_T.tv_nsec = stx_T.tv_nsec;
}

inline void XrdSysStatxHelpers::StatT2StatxT(const struct timespec & sta_T, statx_timestamp & stx_T) {
  stx_T.tv_sec  = sta_T.tv_sec;
  stx_T.tv_nsec = sta_T.tv_nsec;
}

inline void XrdSysStatxHelpers::Stat2Statx(const struct stat & st, XrdSysStatx & stx) {
#ifdef HAVE_STATX
  memset(&stx, 0, sizeof(stx));
  stx.stx_mask      = STATX_BASIC_STATS;
  stx.stx_blksize   = st.st_blksize;
  stx.stx_nlink     = st.st_nlink;
  stx.stx_uid       = st.st_uid;
  stx.stx_gid       = st.st_gid;
  stx.stx_mode      = st.st_mode;
  stx.stx_ino       = st.st_ino;
  stx.stx_size      = st.st_size;
  stx.stx_blocks    = st.st_blocks;
  StatT2StatxT(st.st_atim, stx.stx_atime);
  StatT2StatxT(st.st_mtim, stx.stx_mtime);
  StatT2StatxT(st.st_ctim, stx.stx_ctime);
  stx.stx_dev_major  = major(st.st_dev);
  stx.stx_dev_minor  = minor(st.st_dev);
  stx.stx_rdev_major = major(st.st_rdev);
  stx.stx_rdev_minor = minor(st.st_rdev);
#else
  stx.stx_mask = STATX_BASIC_STATS;
  stx.statx    = st;
#endif
}

inline void XrdSysStatxHelpers::Statx2Stat(const XrdSysStatx & stx, struct stat & st) {
#ifdef HAVE_STATX
  memset(&st, 0, sizeof(st));
  st.st_blksize = stx.stx_blksize;
  st.st_nlink   = stx.stx_nlink;
  st.st_uid     = stx.stx_uid;
  st.st_gid     = stx.stx_gid;
  st.st_mode    = stx.stx_mode;
  st.st_ino     = stx.stx_ino;
  st.st_size    = stx.stx_size;
  st.st_blocks  = stx.stx_blocks;
  StatxT2StatT(stx.stx_atime, st.st_atim);
  StatxT2StatT(stx.stx_mtime, st.st_mtim);
  StatxT2StatT(stx.stx_ctime, st.st_ctim);
  st.st_dev  = makedev(stx.stx_dev_major, stx.stx_dev_minor);
  st.st_rdev = makedev(stx.stx_rdev_major, stx.stx_rdev_minor);
#else
  st = stx.statx;
#endif
}

#endif //XROOTD_XRDSYSSTATX_HH

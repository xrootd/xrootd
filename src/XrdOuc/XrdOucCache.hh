#ifndef __XRDOUCCACHE_HH__
#define __XRDOUCCACHE_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c C a c h e . h h                         */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <cstdint>
#include <vector>

#include "XrdOuc/XrdOucCacheStats.hh"
#include "XrdOuc/XrdOucIOVec.hh"

struct stat;
class  XrdOucEnv;

/******************************************************************************/
/*                       X r d O u c C a c h e I O C B                        */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! The XrdOucCacheIOCB defines a callback object that must be used to handle
//! asynchronous I/O operations.
//-----------------------------------------------------------------------------

class XrdOucCacheIOCB
{
public:

//------------------------------------------------------------------------------
//! Handle result from a previous async operation.
//!
//! @param result is result from a previous operation. Successful results are
//!               always values >= 0 while errors are negative values and are
//!               always '-errno' indicate the reason for the error.
//------------------------------------------------------------------------------
virtual
void     Done(int result) = 0;

         XrdOucCacheIOCB() {}
virtual ~XrdOucCacheIOCB() {}
};

/******************************************************************************/
/*                       X r d O u c C a c h e I O C D                        */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! The XrdOucCacheIOCD defines a callback object that is used to handle
//! XrdOucCacheIO::Detach() requests. It is passed to Detach() and if Detach()
//! returns false then the DetachDone() method must be called when the object
//! has been successfully detached from the cache.
//-----------------------------------------------------------------------------

class XrdOucCacheIOCD
{
public:

//------------------------------------------------------------------------------
//! Indicate that the CacheIO object has been detached.
//------------------------------------------------------------------------------
virtual
void     DetachDone() = 0;

         XrdOucCacheIOCD() {}
virtual ~XrdOucCacheIOCD() {}
};
  
/******************************************************************************/
/*                   C l a s s   X r d O u c C a c h e I O                    */
/******************************************************************************/

//------------------------------------------------------------------------------
//! The XrdOucCacheIO object is responsible for interacting with the original
//! data source/target. It can be used with or without a front-end cache.
//------------------------------------------------------------------------------

class XrdOucCacheIO
{
public:

//------------------------------------------------------------------------------
//! Detach this CacheIO object from the cache.
//!
//! @note   This method must be called instead of using the delete operator
//!         since CacheIO objects may have multiple outstanding references and
//!         actual deletion may need to be deferred.
//!
//! @param  iocd   reference to the detach complete callback object.
//!
//! @return true   Deletion can occur immediately. There is no outstanding I/O.
//! @return false  Deletion must be deferred until it is safe to so from the
//!                cache perspective. At which point, the cache will call the
//!                DetachDone() method in the passed callback object. No locks
//!                may be held with respect to the CacheIO object when this is
//!                done to avoid deadlocks.
//------------------------------------------------------------------------------

virtual bool Detach(XrdOucCacheIOCD &iocd) = 0;

//------------------------------------------------------------------------------
//! Obtain size of the file.
//!
//! @return Size of the file in bytes.
//------------------------------------------------------------------------------
virtual
long long    FSize() = 0;

//------------------------------------------------------------------------------
//! Perform an fstat() operation (defaults to passthrough).
//!
//! @param  sbuff  reference to the stat buffer to be filled in. Only fields
//!                st_size, st_blocks, st_mtime (st_atime and st_ctime may be
//!                set to st_mtime), st_ino, and st_mode need to be set. All
//!                other fields are preset and should not be changed.
//!
//! @return <0 - fstat failed, value is -errno.
//!         =0 - fstat succeeded, sbuff holds stat information.
//!         >0 - fstat could not be done, forward operation to next level.
//------------------------------------------------------------------------------

virtual int  Fstat(struct stat &sbuff) {(void)sbuff; return 1;}

//-----------------------------------------------------------------------------
//! Get the file's location (i.e. endpoint hostname and port)
//!
//! @param  refresh - when true, recomputes the location in case it changed st
//!                   the location is cached from the previous successful call.
//!
//! @return A pointer to the file's location. It remains valid until the file
//!         is closed or Location() is called with refresh set to true.
//!         A null string means the file is not open or location is unknown.
//-----------------------------------------------------------------------------
virtual
const char  *Location(bool refresh=false) {(void)refresh; return "";}

//------------------------------------------------------------------------------
//! Get the path associated with this object.
//!
//! @return Pointer to the path.
//------------------------------------------------------------------------------
virtual
const char  *Path() = 0;

//-----------------------------------------------------------------------------
//! Read file pages into a buffer and return corresponding checksums.
//!
//! @param  buff  pointer to buffer where the bytes are to be placed.
//! @param  offs  The offset where the read is to start.
//! @param  rdlen The number of bytes to read.
//! @param  csvec A vector whose entries which will be filled with the
//!               corresponding CRC32C checksum for each page or pgae segment.
//!               If a zero length vector is returned, checksums are not present.
//! @param  opts  Processing options:
//!               forceCS - always return checksums even when not available.
//! @param  csfix When not nil, returns the number of corrected checksum errs.
//!
//! @return >= 0      The number of bytes placed in buffer.
//! @return -errno    File could not be read, return value is the reason.
//-----------------------------------------------------------------------------

static const uint64_t forceCS = 0x0000000000000001ULL;

virtual int  pgRead(char                  *buff,
                    long long              offs,
                    int                    rdlen,
                    std::vector<uint32_t> &csvec,
                    uint64_t               opts=0,
                    int                   *csfix=0);

//-----------------------------------------------------------------------------
//! Read file pages and checksums using asynchronous I/O (default sync).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  buff  pointer to buffer where the bytes are to be placed.
//! @param  offs  The offset where the read is to start.
//! @param  rdlen The number of bytes to read.
//! @param  csvec A vector which will be filled with the corresponding
//!               CRC32C checksum for each page or page segment.
//! @param  opts  Processing options:
//!               forceCS - always return checksums even when not available.
//! @param  csfix When not nil, returns the number of corrected checksum errs.
//-----------------------------------------------------------------------------

virtual void pgRead(XrdOucCacheIOCB       &iocb,
                    char                  *buff,
                    long long              offs,
                    int                    rdlen,
                    std::vector<uint32_t> &csvec,
                    uint64_t               opts=0,
                    int                   *csfix=0)
                   {iocb.Done(pgRead(buff, offs, rdlen, csvec, opts, csfix));}

//-----------------------------------------------------------------------------
//! Write file pages from a buffer and corresponding verified checksums.
//!
//! @param  buff  pointer to buffer holding the bytes to be written.
//! @param  offs  The offset where the write is to start.
//! @param  wrlen The number of bytes to write.
//!               offs+wrlen (i.e. it establishes an end of file).
//! @param  csvec A vector of that holds the corresponding verified CRC32C
//!               checksum for each page or page segment.
//! @param  opts  Processing options.
//! @param  csfix When not nil, returns the number of corrected checksum errs.
//!
//! @return >= 0      The number of bytes written.
//! @return -errno    File could not be written, returned value is the reason.
//-----------------------------------------------------------------------------

virtual int  pgWrite(char                  *buff,
                     long long              offs,
                     int                    wrlen,
                     std::vector<uint32_t> &csvec,
                     uint64_t               opts=0,
                     int                   *csfix=0);

//-----------------------------------------------------------------------------
//! Write file pages and checksums using asynchronous I/O (default sync).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  buff  pointer to buffer holding the bytes to be written.
//! @param  offs  The offset where the write is to start.
//! @param  wrlen The number of bytes to write.
//! @param  csvec A vector of that holds the corresponding verified CRC32C
//!               checksum for each page or page segment.
//! @param  opts  Processing options.
//! @param  csfix When not nil, returns the number of corrected checksum errs.
//-----------------------------------------------------------------------------

virtual void pgWrite(XrdOucCacheIOCB       &iocb,
                     char                  *buff,
                     long long              offs,
                     int                    wrlen,
                     std::vector<uint32_t> &csvec,
                     uint64_t               opts=0,
                     int                   *csfix=0)
                    {iocb.Done(pgWrite(buff, offs, wrlen, csvec, opts, csfix));}

//------------------------------------------------------------------------------
//! Perform an asynchronous preread (may be ignored).
//!
//! @param  offs  the offset into the file.
//! @param  rlen  the number of bytes to preread into the cache.
//! @param  opts  one or more of the options defined below.
//------------------------------------------------------------------------------

static const int SingleUse = 0x0001; //!< Mark pages for single use

virtual void Preread(long long offs, int rlen, int opts=0)
                    {(void)offs; (void)rlen; (void)opts;}

//-----------------------------------------------------------------------------
//! Set automatic preread parameters for this file (may be ignored).
//!
//! @param  aprP Reference to preread parameters.
//-----------------------------------------------------------------------------

struct aprParms
      {int   Trigger;   // preread if (rdln < Trigger)        (0 -> pagesize+1)
       int   prRecalc;  // Recalc pr efficiency every prRecalc bytes   (0->50M)
       int   Reserve1;
       short minPages;  // If rdln/pgsz < min,  preread minPages       (0->off)
       signed
       char  minPerf;   // Minimum auto preread performance required   (0->n/a)
       char  Reserve2;
       void *Reserve3;

             aprParms() : Trigger(0),  prRecalc(0), Reserve1(0),
                          minPages(0), minPerf(90), Reserve2(0), Reserve3(0) {}
      };

virtual void Preread(aprParms &Parms) { (void)Parms; }

//------------------------------------------------------------------------------
//! Perform an synchronous read.
//!
//! @param  buff  pointer to the buffer to receive the results. The buffer must
//!               remain valid until the callback is invoked.
//! @param  offs  the offset into the file.
//! @param  rlen  the number of bytes to read.
//!
//! @return       < 0 - Read failed, value is -errno.
//!               >=0 - Read succeeded, value is number of bytes read.
//------------------------------------------------------------------------------

virtual int  Read (char *buff, long long offs, int rlen) = 0;

//------------------------------------------------------------------------------
//! Perform an asynchronous read (defaults to synchronous).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  buff  pointer to the buffer to receive the results. The buffer must
//!               remain valid until the callback is invoked.
//! @param  offs  the offset into the file.
//! @param  rlen  the number of bytes to read.
//------------------------------------------------------------------------------

virtual void Read (XrdOucCacheIOCB &iocb, char *buff, long long offs, int rlen)
                  {iocb.Done(Read(buff, offs, rlen));}

//------------------------------------------------------------------------------
//! Perform an synchronous vector read.
//!
//! @param  readV pointer to a vector of read requests.
//! @param  rnum  the number of elements in the vector.
//!
//! @return       < 0 - ReadV failed, value is -errno.
//!               >=0 - ReadV succeeded, value is number of bytes read.
//------------------------------------------------------------------------------

virtual int  ReadV(const XrdOucIOVec *readV, int rnum);

//------------------------------------------------------------------------------
//! Perform an asynchronous vector read (defaults to synchronous).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  readV pointer to a vector of read requests.
//! @param  rnum  the number of elements in the vector.
//------------------------------------------------------------------------------

virtual void ReadV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV, int rnum)
                  {iocb.Done(ReadV(readV, rnum));}

//------------------------------------------------------------------------------
//! Perform an synchronous sync() operation.
//!
//! @return       <0 - Sync failed, value is -errno.
//!               =0 - Sync succeeded.
//------------------------------------------------------------------------------

virtual int  Sync() = 0;

//------------------------------------------------------------------------------
//! Perform an asynchronous sync() operation (defaults to synchronous).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//------------------------------------------------------------------------------

virtual void Sync(XrdOucCacheIOCB &iocb) {iocb.Done(Sync());}

//------------------------------------------------------------------------------
//! Perform an synchronous trunc() operation.
//!
//! @param  offs  the size the file is have.
//!
//! @return       <0 - Trunc failed, value is -errno.
//!               =0 - Trunc succeeded.
//------------------------------------------------------------------------------

virtual int  Trunc(long long offs) = 0;

//------------------------------------------------------------------------------
//! Perform an asynchronous trunc() operation (defaults to synchronous).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  offs  the size the file is have.
//------------------------------------------------------------------------------

virtual void Trunc(XrdOucCacheIOCB &iocb, long long offs)
                  {iocb.Done(Trunc(offs));}

//------------------------------------------------------------------------------
//! Update the originally passed XrdOucCacheIO object with the object passed.
//! All future uses underlying XrdOucCacheIO object must now use this object.
//! Update() is called when Prepare() indicated that the file should not be
//! physically opened and a file method was invoked in the XrdOucCacheIO
//! passed to Attach(). When this occurs, the file is actually opened and
//! Update() called to replace the original XrdOucCacheIO object with one
//! that uses the newly opened file.
//!
//! @param iocp   reference to the new XrdOucCacheIO object.
//------------------------------------------------------------------------------

virtual void Update(XrdOucCacheIO &iocp) {}

//------------------------------------------------------------------------------
//! Perform an synchronous write.
//!
//! @param  buff  pointer to the buffer holding the contents. The buffer must
//!               remain valid until the callback is invoked.
//! @param  offs  the offset into the file.
//! @param  wlen  the number of bytes to write
//!
//! @return       < 0 - Write failed, value is -errno.
//!               >=0 - Write succeeded, value is number of bytes written.
//------------------------------------------------------------------------------

virtual int  Write(char *buff, long long offs, int wlen) = 0;

//------------------------------------------------------------------------------
//! Perform an asynchronous write (defaults to synchronous).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  buff  pointer to the buffer holding the contents. The buffer must
//!               remain valid until the callback is invoked.
//! @param  offs  the offset into the file.
//! @param  wlen  the number of bytes to write
//------------------------------------------------------------------------------

virtual void Write(XrdOucCacheIOCB &iocb, char *buff, long long offs, int wlen)
                  {iocb.Done(Write(buff, offs, wlen));}

//------------------------------------------------------------------------------
//! Perform an synchronous vector write.
//!
//! @param  writV pointer to a vector of write requests.
//! @param  wnum  the number of elements in the vector.
//!
//! @return       < 0 - WriteV failed, value is -errno.
//!               >=0 - WriteV succeeded, value is number of bytes written.
//------------------------------------------------------------------------------

virtual int  WriteV(const XrdOucIOVec *writV, int wnum);

//------------------------------------------------------------------------------
//! Perform an asynchronous vector write (defaults to synchronous).
//!
//! @param  iocb  reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//! @param  writV pointer to a vector of read requests.
//! @param  wnum  the number of elements in the vector.
//------------------------------------------------------------------------------

virtual void WriteV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *writV, int wnum)
                  {iocb.Done(WriteV(writV, wnum));}

//------------------------------------------------------------------------------
//! Construct and Destructor
//------------------------------------------------------------------------------

            XrdOucCacheIO() {}
protected:
virtual    ~XrdOucCacheIO() {}  // Always use Detach() instead of direct delete!
};

/******************************************************************************/
/*                     C l a s s   X r d O u c C a c h e                      */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! The XrdOucCache class is used to define a cache. The cache is associated
//! with one or more XrdOucCacheIO objects using the Attach() method.
//------------------------------------------------------------------------------

class XrdOucCache
{
public:

//------------------------------------------------------------------------------
//! Obtain a new XrdOucCacheIO object that fronts an existing XrdOucCacheIO
//! with this cache. Upon success a pointer to a new XrdOucCacheIO object is
//! returned and must be used to read and write data with the cache interposed.
//! Upon failure, the original XrdOucCacheIO object is returned with errno set.
//! You can continue using the object without any cache. The new cache should
//! use the methods in the passed CacheIO object to perform I/O operatios.
//!
//! @param  ioP     Pointer to the current CacheIO object used for I/O.
//! @param  opts    Cache options as defined below. When specified, they
//!                 override the default options associated with the cache,
//!                 except for optRW, optNEW, and optWIN which are valid only
//!                 for a r/w cache.
//!
//! @return Pointer to a new XrdOucCacheIO object (success) or the original
//!         XrdOucCacheIO object (failure) with errno set.
//------------------------------------------------------------------------------

static const int optFIS = 0x0001; //!< File is structured   (e.g. root file)
static const int optRW  = 0x0004; //!< File is read/write   (o/w read/only)
static const int optNEW = 0x0014; //!< File is new -> optRW (o/w read or write)
static const int optWIN = 0x0024; //!< File is new -> optRW use write-in cache

virtual
XrdOucCacheIO *Attach(XrdOucCacheIO *ioP, int opts=0) = 0;

//------------------------------------------------------------------------------
//! Get the path to a file that is complete in the local cache. By default, the
//! file must be complete in the cache (i.e. no blocks are missing). This can
//! be overridden. Thes path can be used to access the file on the local node.
//!
//! @param  url    - Pointer to the url of interest.
//! @param  buff   - Pointer to a buffer to receive the local path to the file.
//!                  If nil, no path is returned.
//! @param  blen   - Length of the buffer, buff. If zero, no path is returned.
//! @param  why    - One of the LFP_Reason enums describing the call:
//!                  ForAccess - the path will be used to access the file. If
//!                              the file is complete, the system will delay
//!                              purging the file for a configurable window,
//!                              should a purge be imminent. A null path is
//!                              returned for any non-zero return code.
//!                  ForInfo   - same as ForAccess except that purging will
//!                              not be delayed if imminent. A path is always
//!                              returned, if possible. Otherwise the first
//!                              byte of any supplied buffer is set to 0.
//!                  ForPath   - Only the path is wanted and no checks need
//!                              be performed. The only possible errors are
//!                              -EINVAL and -ENAMETOOLONG.
//! @param  forall - When ForAccess is specified: when true makes the file
//!                  world readable; otherwise, only group readable. The
//!                  parameter is ignored unless "why" is ForAccess and a
//!                  local file path is requested to be returned.
//!
//! @return 0      - the file is complete and the local path to the file is in
//!                  the buffer, if it has been supllied.
//!
//! @return <0     - the request could not be fulfilled. The return value is
//!                  -errno describing why. If a buffer was supplied and a
//!                  path could be generated it is returned only if "why" is
//!                  ForInfo or ForPath. Otherwise, a null path is returned.
//!                  
//!                  Common return codes are:
//!                  -EINVAL       an argument is invalid.
//!                  -EISDIR       target is a directory not a file.
//!                  -ENAMETOOLONG buffer not big enough to hold path.
//!                  -ENOENT       file not in cache
//!                  -ENOTSUP      method not implemented
//!                  -EREMOTE      file is incomplete
//!
//! @return >0     - Reserved for future use.
//------------------------------------------------------------------------------

enum LFP_Reason {ForAccess=0, ForInfo, ForPath};

virtual int    LocalFilePath(const char *url, char *buff=0, int blen=0,
                             LFP_Reason why=ForAccess, bool forall=false)
                             {(void)url; (void)buff; (void)blen; (void)why;
                              (void)forall;
                             if (buff && blen > 0) *buff = 0;
                              return -ENOTSUP;
                             }

//------------------------------------------------------------------------------
//! Prepare the cache for a file open request. This method is called prior to
//! actually opening a file. This method is meant to allow defering an open
//! request or implementing the full I/O stack in the cache layer.
//!
//! @param  url    - Pointer to the url about to be opened.
//! @param  oflags - Standard Unix open flags (see open(2)).
//! @param  mode   - Standard mode flags if file is being created.
//!
//! @return <0 Error has occurred, return value is -errno; fail open request. 
//!            The error code -EUSERS may be returned to trigger overload
//!            recovery as specified by the xrootd.fsoverload directive. No
//!            other method should return this error code.
//!         =0 Continue with open() request.
//!         >0 Defer open but treat the file as actually being open.
//------------------------------------------------------------------------------

virtual int    Prepare(const char *url, int oflags, mode_t mode)
                      {(void)url; (void)oflags; (void)mode; return 0;}

//------------------------------------------------------------------------------
//! Rename a file in the cache.
//!
//! @param  oldp - the existing path to be renamed.
//! @param  newp - the new name it is to have.
//!
//! @return Success: 0
//! @return Failure: -errno
//------------------------------------------------------------------------------

virtual int   Rename(const char* oldp, const char* newp)
                    {(void)oldp; (void)newp; return 0;}

//------------------------------------------------------------------------------
//! Remove a directory from the cache.
//!
//! @param  dirp - the existing directory path to be removed.
//!
//! @return Success: 0
//! @return Failure: -errno
//------------------------------------------------------------------------------

virtual int   Rmdir(const char* dirp) {(void)dirp; return 0;}

//------------------------------------------------------------------------------
//! Perform a stat() operation (defaults to passthrough).
//!
//! @param url    pointer to the url whose stat information is wanted.
//! @param sbuff  reference to the stat buffer to be filled in. Only fields
//!               st_size, st_blocks, st_mtime (st_atime and st_ctime may be
//!               set to st_mtime), st_ino, and st_mode need to be set. All
//!               other fields are preset and should not be changed.
//!
//! @return <0 - Stat failed, value is -errno.
//!         =0 - Stat succeeded, sbuff holds stat information.
//!         >0 - Stat could not be done, forward operation to next level.
//------------------------------------------------------------------------------

virtual int  Stat(const char *url, struct stat &sbuff)
                 {(void)url; (void)sbuff; return 1;}

//------------------------------------------------------------------------------
//! Truncate a file in the cache to a specified size.
//!
//! @param  path - the path of the file to truncate.
//! @param  size - the size in bytes it is to have.
//!
//! @return Success: 0
//! @return Failure: -errno
//------------------------------------------------------------------------------

virtual int   Truncate(const char* path, off_t size)
                      {(void)path; (void)size; return 0;}

//------------------------------------------------------------------------------
//! Remove a file from the cache.
//!
//! @param  path - the path of the file to be removed.
//!
//! @return Success: 0
//! @return Failure: -errno
//------------------------------------------------------------------------------

virtual int   Unlink(const char* path) {(void)path; return 0;}

//------------------------------------------------------------------------------
//! Perform special operation on the cache.
//!
//! @param cmd     - The operation to be performed.
//! @param arg     - The operation argument(s).
//! @param arglen  - The length of arg.
//!
//! @return Success: 0
//! @return Failure: -errno
//------------------------------------------------------------------------------

enum XeqCmd {xeqNoop = 0};

virtual int    Xeq(XeqCmd cmd, char *arg, int arglen)
                  {(void)cmd; (void)arg; (void)arglen; return -ENOTSUP;}

//------------------------------------------------------------------------------
//! The following holds statistics for the cache itself. It is updated as
//! associated cacheIO objects are deleted and their statistics are added.
//------------------------------------------------------------------------------

XrdOucCacheStats Statistics;

//------------------------------------------------------------------------------
//! A 1-to-7 character cache type identifier (usually pfc or rmc).
//------------------------------------------------------------------------------

const char CacheType[8];

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param ctype   - A 1-to-7 character cache type identifier.
//------------------------------------------------------------------------------

               XrdOucCache(const char *ctype) : CacheType{}
//                        : CacheType({'\0','\0','\0','\0','\0','\0','\0','\0'})
                          {strncpy(const_cast<char *>(CacheType), ctype,
                                   sizeof(CacheType));
                           const_cast<char *>(CacheType)[sizeof(CacheType)-1]=0;
                          }

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual       ~XrdOucCache() {}
};

/******************************************************************************/
/*               C r e a t i n g   C a c h e   P l u g - I n s                */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Your cache plug-in must exist in a shared library and have the following
//! extern C function defined whose parameters are:
//!
//! @param Logger  Pointer to the logger object that should be used with an
//!                instance of XrdSysError to direct messages to a log file.
//!                If Logger is null, you should use std::cerr to output messages.
//! @param Config  Pointer to the configuration file name from where you
//!                should get additional information. If Config is null, there
//!                is no configuration file is present.
//! @param Parms   Pointer to any parameters specified after the shared library
//!                path. If Parms is null, there are no parameters.
//! @param envP    Pointer to environmental information. The most relevant
//!                is whether or not gStream monitoring is enabled.
//!                @code {.cpp}
//!                XrdXrootdGStream *gStream = (XrddXrootdGStream *)
//!                                            envP->GetPtr("pfc.gStream*");
//!                @endcode
//! @return        A usable, fully configured, instance of an XrdOucCache
//!                object upon success and a null pointer otherwise. This
//!                instance is used for all operations defined by methods in
//!                XrdOucCache base class.
//!
//! @code {.cpp}
//! extern "C"
//! {
//! XrdOucCache *XrdOucGetCache(XrdSysLogger *Logger, // Where messages go
//!                             const char   *Config, // Config file used
//!                             const char   *Parms,  // Optional parm string
//!                             XrdOucEnv    *envP);  // Optional environment
//! }
//! @endcode

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. Declare it as shown below.
//------------------------------------------------------------------------------

/*!
        #include "XrdVersion.hh"
        XrdVERSIONINFO(XrdOucGetCache,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/

typedef XrdOucCache *(*XrdOucCache_t)(XrdSysLogger *Logger, const char *Config,
                                      const char   *Parms,  XrdOucEnv  *envP);

#endif

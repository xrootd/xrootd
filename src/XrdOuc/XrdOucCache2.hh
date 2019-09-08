#ifndef __XRDOUCCACHE2_HH__
#define __XRDOUCCACHE2_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c C a c h e 2 . h h                        */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucCache.hh"

//-----------------------------------------------------------------------------
//! XrdOucCache2
//!
//! This class is an extension of the original XrdOucCache class and provides
//! async I/O support, defered open and I/O decoupling. It is loaded as a
//! cache plugin by the POSIX package via the proxy directive
//!
//! pss.cache -2 \<path\>
//!
//! Without the "-2" the original version is loaded. Implementation of a cache
//! is similar between versions (see XrdOucCache.hh for details). The only
//! difference is this version requires additional methods to be implemented
//! and uses an asynchrnous callback mechanism to return the results.
//-----------------------------------------------------------------------------
  
/******************************************************************************/
/*                  C l a s s   X r d O u c C a c h e I O 2                   */
/******************************************************************************/

//------------------------------------------------------------------------------
//! The XrdOucCacheIO2 object is responsible for interacting with the original
//! data source/target. It can be used with or without a front-end cache. It
//! an extension of the XrdOucCacheIO class which defines base methods.
//------------------------------------------------------------------------------

class XrdOucCacheIO2 : public virtual XrdOucCacheIO
{
public:

//------------------------------------------------------------------------------
//! Perform an fstat() operation (defaults to passthrough).
//!
//! @param sbuff  reference to the stat buffer to be filled in. Only fields
//!               st_size, st_blocks, st_mtime (st_atime and st_ctime may be
//!               set to st_mtime), st_ino, and st_mode need to be set. All
//!               other fields are preset and should not be changed.
//!
//! @return <0 - fstat failed, value is -errno.
//!         =0 - fstat succeeded, sbuff holds stat information.
//!         >0 - fstat could not be done, forward operation to next level.
//------------------------------------------------------------------------------

virtual int  Fstat(struct stat &sbuff) {(void)sbuff; return 1;}

//-----------------------------------------------------------------------------
//! Get the file's location (i.e. endpoint hostname and port)
//!
//! @return A pointer to the file's location. It remains valid until the file
//!         is closed. A null string means the file is not open or is unknown.
//-----------------------------------------------------------------------------
virtual
const char  *Location() {return "";}

//------------------------------------------------------------------------------
//! Perform an asynchronous read (defaults to synchrnous).
//!
//! @param iocb   reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread. Done() is passed
//!               < 0 - Read failed, value is -errno.
//!               >=0 - Read succeeded, value is number of bytes read.
//! @param buff   pointer to the buffer to receive the results. The buffer must
//!               remain valid until the callback is invoked.
//! @param offs   the offset into the file.
//! @param rlen   the number of bytes to read.
//------------------------------------------------------------------------------

using        XrdOucCacheIO::Read;

virtual void Read (XrdOucCacheIOCB &iocb, char *buff, long long offs, int rlen)
                  {iocb.Done(Read(buff, offs, rlen));}

//------------------------------------------------------------------------------
//! Perform an asynchronous vector read (defaults to synchrnous).
//!
//! @param iocb   reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread. Done() is passed
//!               < 0 - ReadV failed, value is -errno.
//!               >=0 - ReadV succeeded, value is number of bytes read.
//! @param readV  pointer to a vector of read requests.
//! @param rnum   the number of elements in the vector.
//------------------------------------------------------------------------------

using        XrdOucCacheIO::ReadV;

virtual void ReadV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV, int rnum)
                  {iocb.Done(ReadV(readV, rnum));}

//------------------------------------------------------------------------------
//! Perform an asynchronous fsync() operation (defaults to synchronous).
//!
//! @param iocb   reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread. Done() is passed
//!               <0 - Sync failed, value is -errno.
//!               =0 - Sync succeeded.
//------------------------------------------------------------------------------

using        XrdOucCacheIO::Sync;

virtual void Sync(XrdOucCacheIOCB &iocb) {iocb.Done(Sync());}

//------------------------------------------------------------------------------
//! Update the originally passed XrdOucCacheIO2 object with the object passed.
//! All future uses underlying XrdOucCacheIO2 object must now use this object.
//! Update() is called when Prepare() indicated that the file should not be
//! physically opened and a file method was invoked in the XrdOucCacheIO2
//! passed to Attach(). When this occurs, the file is actually opened and
//! Update() called to replace the original XrdOucCacheIO2 object with one
//! that uses the newly opened file.
//!
//! @param iocp   reference to the new XrdOucCacheIO2 object.
//------------------------------------------------------------------------------

virtual void Update(XrdOucCacheIO2 &iocp) {}

//------------------------------------------------------------------------------
//! Perform an asynchronous write (defaults to synchronous).
//!
//! @param iocb   reference to the callback object that receives the result. All
//!               results are returned via this object's Done() method. If the
//!               caller holds any locks they must be recursive locks as the
//!               callback may occur on the calling thread.
//!               < 0 - Write failed, value is -errno.
//!               >=0 - Write succeeded, value is number of bytes written.
//! @param buff   pointer to the buffer holding the contents. The buffer must
//!               remain valid until the callback is invoked.
//! @param offs   the offset into the file.
//! @param wlen   the number of bytes to write
//------------------------------------------------------------------------------

using        XrdOucCacheIO::Write;

virtual void Write(XrdOucCacheIOCB &iocb, char *buff, long long offs, int wlen)
                  {iocb.Done(Write(buff, offs, wlen));}

//------------------------------------------------------------------------------

virtual    ~XrdOucCacheIO2() {}  // Always use Detach() instead of direct delete!
};

/******************************************************************************/
/*                    C l a s s   X r d O u c C a c h e 2                     */
/******************************************************************************/

class  XrdOucEnv;
struct stat;
  
//------------------------------------------------------------------------------
//! The XrdOucCache2 class is used to define a version 2 cache. In version 2,
//! there can be only one such instance. the cache is associated with one or
//! more  XrdOucCacheIO2 objects. Use the Attach() method in this class to
//! create such associations.
//------------------------------------------------------------------------------

class XrdOucCache2 : public virtual XrdOucCache
{
public:

//------------------------------------------------------------------------------
//! Obtain a new XrdOucCacheIO2 object that fronts an existing XrdOucCacheIO2
//! with this cache. Upon success a pointer to a new XrdOucCacheIO2 object is
//! returned and must be used to read and write data with the cache interposed.
//! Upon failure, the original XrdOucCacheIO2 object is returned with errno set.
//! You can continue using the object without any cache. The new cache should
//! use the methods in the passed CacheIO2 object to perform I/O operatios.
//!
//! @param  ioP     Pointer to the current CacheIO2 object used for I/O.
//! @param  opts    Cache options identical to those defined for XrdOucCache
//!                 Attach() method.
//!
//! @return Pointer to a new XrdOucCacheIO2 object (success) or the original
//!         XrdOucCacheIO2 object (failure) with errno set.
//------------------------------------------------------------------------------

using XrdOucCache::Attach;

virtual
XrdOucCacheIO2 *Attach(XrdOucCacheIO2 *ioP, int opts=0) = 0;

virtual
XrdOucCacheIO  *Attach(XrdOucCacheIO  *ioP, int opts=0)
                      {errno = ENOSYS; return ioP;}

//------------------------------------------------------------------------------
//! Creates an instance of a version 1 cache. This method is no longer used so
//! we simply define a default for this method here for backward compatability.
//!
//! @return A pointer to an XrdOucCache object upon success or a nil pointer
//!         with errno set upon failure.
//------------------------------------------------------------------------------
virtual
XrdOucCache   *Create(Parms &Params, XrdOucCacheIO::aprParms *aprP=0)
                     {return this;}

//------------------------------------------------------------------------------
//! Supply environmental information to the cache. This is only called on the
//! server but is optional and might not be called. When it is called, it is
//! gauranteed to occur before any active use of the cache and is essentially
//! serialized (i.e. the main start-up thread is used). The environmental
//! information should only be used to optimize processing. For instance,
//! when cache monitoring is enabled, the variable "pfc.gStream*" is defined
//! and is a pointer to a gStream object that can be used to report statistical
//! information to a monitoring collector.
//!
//! @param  theEnv - Reference to environmental information.
//------------------------------------------------------------------------------
virtual
void           EnvInfo(XrdOucEnv &theEnv) {(void)theEnv;}

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

virtual
int            LocalFilePath(const char *url, char *buff=0, int blen=0,
                             LFP_Reason why=ForAccess, bool forall=false)
                             {(void)url; (void)buff; (void)blen; (void)why;
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
//!         >0 Defer open but treat the file as actually being open. Use the
//!            XrdOucCacheIO2::Open() method to open the file at a later time.
//------------------------------------------------------------------------------
virtual
int            Prepare(const char *url, int oflags, mode_t mode)
                      {(void)url; (void)oflags; (void)mode; return 0;}

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

               XrdOucCache2() {}
virtual       ~XrdOucCache2() {}
};

/******************************************************************************/
/*               C r e a t i n g   C a c h e   P l u g - I n s                */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! You can create a V2 cache plug-in for those parts of the xrootd system that
//! allow a dynamically selectable cache implementation (e.g. the proxy server
//! plug-in supports cache plug-ins via the pss.cachelib). For the proxy server,
//! it preferentially looks for XrdOucGetCache2() and invokes this function if
//! is exists. Otherwise, it uses the XrdOucGetCache() function (old version 1).
//!
//! Your plug-in must exist in a shared library and have the following extern C
//! function defined whos parameters are:
//!
//! @param Logger  Pointer to the logger object that should be used with an
//!                instance of XrdSysError to direct messages to a log file.
//!                If Logger is null, you should use cerr to output messages.
//! @param Config  Pointer to the configuration file name from where you
//!                should get additional information. If Config is null, there
//!                is no configuration file is present.
//! @param Parms   Pointer to any parameters specified after the shared library
//!                path. If Parms is null, there are no parameters.
//! @return        A usable, fully configured, instance of an XrdOucCache2
//!                object upon success and a null pointer otherwise. This
//!                instance is used for all operations defined by methods in
//!                XrdOucCache bas class as well as this class.
//!
//! extern "C"
//! {
//! XrdOucCache2 *XrdOucGetCache2(XrdSysLogger *Logger, // Where messages go
//!                               const char   *Config, // Config file used
//!                               const char   *Parms); // Optional parm string
//! }
#endif

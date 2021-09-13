#ifndef _XRDOSS_H
#define _XRDOSS_H
/******************************************************************************/
/*                                                                            */
/*                             X r d O s s . h h                              */
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

#include <dirent.h>
#include <cerrno>
#include <cstdint>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstring>

#include "XrdOss/XrdOssVS.hh"
#include "XrdOuc/XrdOucIOVec.hh"

class XrdOucEnv;
class XrdSysLogger;
class XrdSfsAio;

#ifndef XrdOssOK
#define XrdOssOK 0
#endif

/******************************************************************************/
/*                        C l a s s   X r d O s s D F                         */
/******************************************************************************/

//! This class defines the object that handles directory as well as file
//! oriented requests. It is instantiated for each file/dir to be opened.
//! The object is obtained by calling newDir() or newFile() in class XrdOss.
//! This allows flexibility on how to structure an oss plugin.
  
class XrdOssDF
{
public:

/******************************************************************************/
/*            D i r e c t o r y   O r i e n t e d   M e t h o d s             */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! Open a directory.
//!
//! @param  path   - Pointer to the path of the directory to be opened.
//! @param  env    - Reference to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Opendir(const char *path, XrdOucEnv &env) {return -ENOTDIR;}

//-----------------------------------------------------------------------------
//! Get the next directory entry.
//!
//! @param  buff   - Pointer to buffer where a null terminated string of the
//!                  entry name is to be returned. If no more entries exist,
//!                  a null string is returned.
//! @param  blen   - Length of the buffer.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Readdir(char *buff, int blen) {return -ENOTDIR;}

//-----------------------------------------------------------------------------
//! Set the stat() buffer where stat information is to be placed corresponding
//! to the directory entry returned by Readdir().
//!
//! @param  buff   - Pointer to stat structure to be used.
//!
//! @return 0 upon success or -ENOTSUP if not supported.
//!
//! @note This is a one-time call as stat structure is reused for each Readdir.
//! @note When StatRet() is in effect, directory entries that have been
//!       deleted from the target directory are quietly skipped.
//-----------------------------------------------------------------------------

virtual int     StatRet(struct stat *) {return -ENOTSUP;}

/******************************************************************************/
/*                 F i l e   O r i e n t e d   M e t h o d s                  */
/******************************************************************************/
//-----------------------------------------------------------------------------
//! Change file mode settings.
//!
//! @param  mode   - The new file mode setting.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fchmod(mode_t mode) {return -EISDIR;}

//-----------------------------------------------------------------------------
//! Flush filesystem cached pages for this file (used for checksums).
//-----------------------------------------------------------------------------

virtual void    Flush() {}

//-----------------------------------------------------------------------------
//! Return state information for this file.
//!
//! @param  buf    - Pointer to the structure where info it to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fstat(struct stat *buf) {return -EISDIR;}

//-----------------------------------------------------------------------------
//! Synchronize associated file with media (synchronous).
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fsync() {return -EISDIR;}

//-----------------------------------------------------------------------------
//! Synchronize associated file with media (asynchronous).
//!
//! @param  aiop   - Pointer to async I/O request object.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fsync(XrdSfsAio *aiop) {return -EISDIR;}

//-----------------------------------------------------------------------------
//! Set the size of the associated file.
//!
//! @param  flen   - The new size of the file.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Ftruncate(unsigned long long) {return -EISDIR;}

//-----------------------------------------------------------------------------
//! Return the memory mapped characteristics of the file.
//!
//! @param  addr   - Pointer to where the memory mapped address is to be returned.
//!
//! @return If mapped, the size of the file is returned and it memory location
//!         is placed in addr. Otherwise, addr is set to zero and zero is
//!         returned. Note that zero length files cannot be memory mapped.
//-----------------------------------------------------------------------------

virtual off_t   getMmap(void **addr) {*addr = 0; return 0;}

//-----------------------------------------------------------------------------
//! Return file compression charectistics.
//!
//! @param  cxidp  - Pointer to where the compression algorithm name returned.
//!
//! @return If the file is compressed, the region size if returned. Otherwise,
//!         zero is returned (file not compressed).
//-----------------------------------------------------------------------------

virtual int     isCompressed(char *cxidp=0) {(void)cxidp; return 0;}

//-----------------------------------------------------------------------------
//! Open a file.
//!
//! @param  path   - Pointer to the path of the file to be opened.
//! @param  Oflag  - Standard open flags.
//! @param  Mode   - File open mode (ignored unless creating a file).
//! @param  env    - Reference to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env)
                    {return -EISDIR;}

//-----------------------------------------------------------------------------
//! Read file pages into a buffer and return corresponding checksums.
//!
//! @param  buffer  - pointer to buffer where the bytes are to be placed.
//! @param  offset  - The offset where the read is to start. It must be
//!                   page aligned.
//! @param  rdlen   - The number of bytes to read. The amount must be an
//!                   integral number of XrdSfsPage::Size bytes.
//! @param  csvec   - A vector of entries to be filled with the cooresponding
//!                   CRC32C checksum for each page. It must be size to
//!                   rdlen/XrdSys::PageSize + (rdlen%XrdSys::PageSize != 0)
//! @param  opts    - Processing options (see below).
//!
//! @return >= 0      The number of bytes that placed in buffer upon success.
//! @return  < 0       -errno or -osserr upon failure. (see XrdOssError.hh).
//-----------------------------------------------------------------------------

// pgRead and pgWrite options as noted.
//
static const uint64_t
Verify       = 0x8000000000000000ULL; //!< all: Verify    checksums
static const uint64_t
doCalc       = 0x4000000000000000ULL; //!< pgw: Calculate checksums

virtual ssize_t pgRead (void* buffer, off_t offset, size_t rdlen,
                        uint32_t* csvec, uint64_t opts);

//-----------------------------------------------------------------------------
//! Read file pages and checksums using asynchronous I/O.
//!
//! @param  aioparm - Pointer to async I/O object controlling the I/O.
//! @param  opts    - Processing options (see above).
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     pgRead (XrdSfsAio* aioparm, uint64_t opts);

//-----------------------------------------------------------------------------
//! Write file pages into a file with corresponding checksums.
//!
//! @param  buffer  - pointer to buffer containing the bytes to write.
//! @param  offset  - The offset where the write is to start.
//! @param  wrlen   - The number of bytes to write.
//! @param  csvec   - A vector which contains the corresponding CRC32 checksum
//!                   for each page. See XrdOucPgrwUtils::csNum() for sizing.
//! @param  opts    - Processing options (see above).
//!
//! @return >= 0      The number of bytes written upon success.
//!                   or -errno or -osserr upon failure. (see XrdOssError.hh).
//! @return  < 0      -errno or -osserr upon failure. (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen,
                        uint32_t* csvec, uint64_t opts);

//-----------------------------------------------------------------------------
//! Write file pages and checksums using asynchronous I/O.
//!
//! @param  aioparm - Pointer to async I/O object controlling the I/O.
//! @param  opts    - Processing options (see above).
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     pgWrite(XrdSfsAio* aoiparm, uint64_t opts);

//-----------------------------------------------------------------------------
//! Preread file blocks into the file system cache.
//!
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to pre-read.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t Read(off_t offset, size_t size) {return (ssize_t)-EISDIR;}

//-----------------------------------------------------------------------------
//! Read file bytes into a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes are to be placed.
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to read.
//!
//! @return >= 0      The number of bytes that placed in buffer.
//! @return  < 0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t Read(void *buffer, off_t offset, size_t size)
                    {return (ssize_t)-EISDIR;}

//-----------------------------------------------------------------------------
//! Read file bytes using asynchronous I/O.
//!
//! @param  aiop    - Pointer to async I/O object controlling the I/O.
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Read(XrdSfsAio *aoip) {(void)aoip; return (ssize_t)-EISDIR;}

//-----------------------------------------------------------------------------
//! Read uncompressed file bytes into a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes are to be placed.
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to read.
//!
//! @return >= 0      The number of bytes that placed in buffer.
//! @return  < 0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t ReadRaw(void *buffer, off_t offset, size_t size)
                       {return (ssize_t)-EISDIR;}

//-----------------------------------------------------------------------------
//! Read file bytes as directed by the read vector.
//!
//! @param  readV     pointer to the array of read requests.
//! @param  rdvcnt    the number of elements in readV.
//!
//! @return >=0       The numbe of bytes read.
//! @return < 0       -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t ReadV(XrdOucIOVec *readV, int rdvcnt);

//-----------------------------------------------------------------------------
//! Write file bytes from a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes reside.
//! @param  offset  - The offset where the write is to start.
//! @param  size    - The number of bytes to write.
//!
//! @return >= 0      The number of bytes that were written.
//! @return <  0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t Write(const void *buffer, off_t offset, size_t size)
                     {return (ssize_t)-EISDIR;}

//-----------------------------------------------------------------------------
//! Write file bytes using asynchronous I/O.
//!
//! @param  aiop    - Pointer to async I/O object controlling the I/O.
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Write(XrdSfsAio *aiop) {(void)aiop; return (ssize_t)-EISDIR;}

//-----------------------------------------------------------------------------
//! Write file bytes as directed by the write vector.
//!
//! @param  writeV    pointer to the array of write requests.
//! @param  wrvcnt    the number of elements in writeV.
//!
//! @return >=0       The numbe of bytes read.
//! @return < 0       -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t WriteV(XrdOucIOVec *writeV, int wrvcnt);

/******************************************************************************/
/*     C o m m o n   D i r e c t o r y   a n d   F i l e   M e t h o d s      */
/******************************************************************************/
//-----------------------------------------------------------------------------
//! Close a directory or file.
//!
//! @param  retsz     If not nil, where the size of the file is to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Close(long long *retsz=0)=0;

//-----------------------------------------------------------------------------
//! Return the underlying object type.
//!
//! @return Type of object.
//-----------------------------------------------------------------------------

// Returned value will have one or more bits set as below.
//
static const uint16_t DF_isDir   = 0x0001;  //!< Object is for a directory
static const uint16_t DF_isFile  = 0x0002;  //!< Object is for a file
static const uint16_t DF_isProxy = 0x0010;  //!< Object is a proxy object

uint16_t        DFType() {return dfType;}

//-----------------------------------------------------------------------------
//! Execute a special operation on the directory or file.
//!
//! @param  cmd    - The operation to be performed:
//!                  Fctl_ckpObj - Obtain checkpoint object for proxy file.
//!                                Argument: None.
//!                                Response: Pointer to XrdOucChkPnt object.
//!                  Fctl_utimes - Set atime and mtime (no response).
//!                                Argument: struct timeval tv[2]
//! @param  alen   - Length of data pointed to by args.
//! @param  args   - Data sent with request, zero if alen is zero.
//! @param  resp   - Where the response is to be set. The caller must call
//!                  delete on the returned object.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

static const int Fctl_ckpObj = 0;
static const int Fctl_utimes = 1;

virtual int     Fctl(int cmd, int alen, const char *args, char **resp=0);

//-----------------------------------------------------------------------------
//! Return the underlying file descriptor.
//!
//! @return -1 if there is no file descriptor or a non-negative FD number.
//-----------------------------------------------------------------------------

virtual int     getFD() {return -1;} // Must override to support sendfile()

//-----------------------------------------------------------------------------
//! Return trace identifier associated with this object.
//!
//! @return Pointer to trace identifier
//-----------------------------------------------------------------------------
virtual
const char     *getTID() {return tident;}

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//!
//! @param  tid    - Pointer to the trace identifier.
//! @param  dftype - The type of the object.
//! @param  fdnum  - The value for the file descriptor.
//-----------------------------------------------------------------------------

                XrdOssDF(const char *tid="", uint16_t dftype=0, int fdnum=-1)
                        : tident(tid), pgwEOF(0), fd(fdnum), dfType(dftype),
                          rsvd(0) {}

virtual        ~XrdOssDF() {}


protected:

const char *tident;  // Trace identifier
off_t       pgwEOF;  // Highest short offset on pgWrite (0 means none yet)
int         fd;      // The associated file descriptor.
uint16_t    dfType;  // Type of this object
short       rsvd;    // Reserved
};

/******************************************************************************/
/*                        X r d O s s   O p t i o n s                         */
/******************************************************************************/

// Options that can be passed to Create()
//
#define XRDOSS_mkpath  0x01
#define XRDOSS_new     0x02
#define XRDOSS_Online  0x04
#define XRDOSS_isPFN   0x10
#define XRDOSS_isMIG   0x20
#define XRDOSS_setnoxa 0x40

// Values returned by Features()
//
#define XRDOSS_HASPGRW 0x0000000000000001ULL
#define XRDOSS_HASFSCS 0x0000000000000002ULL
#define XRDOSS_HASPRXY 0x0000000000000004ULL
#define XRDOSS_HASNOSF 0x0000000000000008ULL
#define XRDOSS_HASCACH 0x0000000000000010ULL
#define XRDOSS_HASNAIO 0x0000000000000020ULL
#define XRDOSS_HASRPXY 0x0000000000000040ULL

// Options that can be passed to Stat()
//
#define XRDOSS_resonly 0x0001
#define XRDOSS_updtatm 0x0002
#define XRDOSS_preop   0x0004

// Commands that can be passed to FSctl
//
#define XRDOSS_FSCTLFA 0x0001
  
/******************************************************************************/
/*                          C l a s s   X r d O s s                           */
/******************************************************************************/
  
class XrdOss
{
public:

//-----------------------------------------------------------------------------
//! Obtain a new director object to be used for future directory requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to an XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------

virtual XrdOssDF *newDir(const char *tident)=0;

//-----------------------------------------------------------------------------
//! Obtain a new file object to be used for a future file requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to an XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------

virtual XrdOssDF *newFile(const char *tident)=0;

//-----------------------------------------------------------------------------
//! Change file mode settings.
//!
//! @param  path   - Pointer to the path of the file in question.
//! @param  mode   - The new file mode setting.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Chmod(const char * path, mode_t mode, XrdOucEnv *envP=0)=0;

//-----------------------------------------------------------------------------
//! Notify storage system that a client has connected.
//!
//! @param  env    - Reference to environmental information.
//-----------------------------------------------------------------------------

virtual void      Connect(XrdOucEnv &env);

//-----------------------------------------------------------------------------
//! Create file.
//!
//! @param  tid    - Pointer to the trace identifier.
//! @param  path   - Pointer to the path of the file to create.
//! @param  mode   - The new file mode setting.
//! @param  env    - Reference to environmental information.
//! @param  opts   - Create options:
//!                  XRDOSS_mkpath - create dir path if it does not exist.
//!                  XRDOSS_new    - the file must not already exist.
//!                  oflags<<8     - open flags shifted 8 bits to the left/
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Create(const char *, const char *, mode_t, XrdOucEnv &,
                         int opts=0)=0;

//-----------------------------------------------------------------------------
//! Notify storage system that a client has disconnected.
//!
//! @param  env    - Reference to environmental information.
//-----------------------------------------------------------------------------

virtual void      Disc(XrdOucEnv &env);

//-----------------------------------------------------------------------------
//! Notify storage system of initialization information (deprecated).
//!
//! @param  envP   - Pointer to environmental information.
//-----------------------------------------------------------------------------

virtual void      EnvInfo(XrdOucEnv *envP);

//-----------------------------------------------------------------------------
//! Return storage system features.
//!
//! @return Storage system features (see XRDOSS_HASxxx flags).
//-----------------------------------------------------------------------------

virtual uint64_t  Features();

//-----------------------------------------------------------------------------
//! Execute a special storage system operation.
//!
//! @param  cmd    - The operation to be performed:
//!                  XRDOSS_FSCTLFA - Perform proxy file attribute operation
//! @param  alen   - Length of data pointed to by args.
//! @param  args   - Data sent with request, zero if alen is zero.
//! @param  resp   - Where the response is to be set, if any.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       FSctl(int cmd, int alen, const char *args, char **resp=0);

//-----------------------------------------------------------------------------
//! Initialize the storage system V1 (deprecated).
//!
//! @param  lp     - Pointer to the message logging object.
//! @param  cfn    - Pointer to the configuration file.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Init(XrdSysLogger *lp, const char *cfn)=0;

//-----------------------------------------------------------------------------
//! Initialize the storage system V2.
//!
//! @param  lp     - Pointer to the message logging object.
//! @param  cfn    - Pointer to the configuration file.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Init(XrdSysLogger *lp, const char *cfn, XrdOucEnv *envP)
                      {return Init(lp, cfn);}

//-----------------------------------------------------------------------------
//! Create a directory.
//!
//! @param  path   - Pointer to the path of the directory to be created.
//! @param  mode   - The directory mode setting.
//! @param  mkpath - When true the path is created if it does not exist.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Mkdir(const char *path, mode_t mode, int mkpath=0,
                        XrdOucEnv  *envP=0)=0;

//-----------------------------------------------------------------------------
//! Relocate/Copy the file at `path' to a new location.
//!
//! @param  tident - -> trace identifier for this operation.
//! @param  path   - -> fully qualified name of the file to relocate.
//! @param  cgName - -> target space name[:path]
//! @param  anchor - Processing directions (see XrdOssReloc.cc example).
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Reloc(const char *tident, const char *path,
                        const char *cgName, const char *anchor=0);

//-----------------------------------------------------------------------------
//! Remove a directory.
//!
//! @param  path   - Pointer to the path of the directory to be removed.
//! @param  opts   - The processing options:
//!                  XRDOSS_Online   - only remove online copy
//!                  XRDOSS_isPFN    - path is already translated.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0)=0;

//-----------------------------------------------------------------------------
//! Rename a file or directory.
//!
//! @param  oPath   - Pointer to the path to be renamed.
//! @param  nPath   - Pointer to the path oPath is to have.
//! @param  oEnvP   - Environmental information for oPath.
//! @param  nEnvP   - Environmental information for nPath.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Rename(const char *oPath, const char *nPath,
                         XrdOucEnv  *oEnvP=0, XrdOucEnv *nEnvP=0)=0;

//-----------------------------------------------------------------------------
//! Return state information on a file or directory.
//!
//! @param  path   - Pointer to the path in question.
//! @param  buff   - Pointer to the structure where info it to be returned.
//! @param  opts   - Options:
//!                  XRDOSS_preop    - this is a stat prior to open.
//!                  XRDOSS_resonly  - only look for resident files.
//!                  XRDOSS_updtatm  - update file access time.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Stat(const char *path, struct stat *buff,
                       int opts=0, XrdOucEnv *envP=0)=0;

//-----------------------------------------------------------------------------
//! Return statistics.
//!
//! @param  buff   - Pointer to the buffer to hold statistics.
//! @param  blen   - Length of the buffer.
//!
//! @return The number of bytes placed in the buffer excluding null byte.
//-----------------------------------------------------------------------------

virtual int       Stats(char *buff, int blen) {(void)buff; (void)blen; return 0;}

//-----------------------------------------------------------------------------
//! Return filesystem physical space information associated with a path.
//!
//! @param  path   - Path in the partition in question.
//! @param  buff   - Pointer to the buffer to hold the information.
//! @param  blen   - Length of the buffer. This is updated with the actual
//!                  number of bytes placed in the buffer as in snprintf().
//! @param  opts   - Options:
//!                  XRDEXP_STAGE - info for stageable space wanted.
//!                  XRDEXP_NOTRW - info for Read/Only space wanted.
//! @param  envP   - Pointer to environmental information.
//!
//! @return "<wval> <fsp> <utl> <sval> <fsp> <utl>"
//!         where: <wval> is "0" if XRDEXP_NOTRW specified, otherwise "1"
//!                <fsp>  is free space in megabytes.
//!                <utl>  is percentage utilization (i.e. allocated space)
//!                <sval> is "1' if XRDEXP_STAGE specified, otherwise "0"
//!         Upon failure -errno or -osserr (see XrdOssError.hh) returned.
//-----------------------------------------------------------------------------

virtual int       StatFS(const char *path, char *buff, int &blen,
                         XrdOucEnv  *envP=0);

//-----------------------------------------------------------------------------
//! Return filesystem physical space information associated with a space name.
//!
//! @param  path   - Path in the name space in question. The space name
//!                  associated with gthe path is used unless overridden.
//! @param  buff   - Pointer to the buffer to hold the information.
//! @param  blen   - Length of the buffer. This is updated with the actual
//!                  number of bytes placed in the buffer as in snprintf().
//! @param  opts   - Options (see StatFS()) apply only when there are no
//!                  spaces defined.
//! @param  envP   - Ref to environmental information. If the environment
//!                  has the key oss.cgroup defined, the associated value is
//!                  used as the space name and the path is ignored.
//!
//! @return "oss.cgroup=<name>&oss.space=<totbytes>&oss.free=<freebytes>
//!          &oss.maxf=<maxcontigbytes>&oss.used=<bytesused>
//!          &oss.quota=<quotabytes>" in buff upon success.
//!         Upon failure -errno or -osserr (see XrdOssError.hh) returned.
//-----------------------------------------------------------------------------

virtual int       StatLS(XrdOucEnv &env, const char *path,
                         char *buff, int &blen);

//-----------------------------------------------------------------------------
//! Return state information on a resident physical file or directory.
//!
//! @param  path   - Pointer to the path in question.
//! @param  buff   - Pointer to the structure where info it to be returned.
//! @param  opts   - Options:
//!                  PF_dInfo - provide bdevID in st_rdev and partID in st_dev
//!                             based on path. If path is nil then the contents
//!                             of the of buff is used as the input source.
//!                  PF_dNums - provide number of bdev's in st_rdev and the
//!                             number of partitions in st_dev. The path
//!                             argument is ignored. This superceeds PF_dInfo.
//!                  PF_dStat - provide file state flags in st_rdev as shown
//!                             below. Path may not be nil. This supercedes
//!                             PF_dInfo and PF_dNums.
//!                  PF_isLFN - Do N2N translation on path (default is none).
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

static const int  PF_dInfo = 0x00000001;
static const int  PF_dNums = 0x00000002;
static const int  PF_isLFN = 0x00000004;
static const int  PF_dStat = 0x00000008;

// Bits returned in st_rdev when PF_dStat specified in opts. Absence of either
// PF_csVer and PF_csVun flags means that the file has no checksums present.
//
static const int  PF_csVer = 0x00000001; //!<   verified file checksums present
static const int  PF_csVun = 0x00000002; //!< unverified file checksums present

virtual int       StatPF(const char *path, struct stat *buff, int opts);

virtual int       StatPF(const char *path, struct stat *buff)
                        {return StatPF(path, buff, 0);} // Backward compat

//-----------------------------------------------------------------------------
//! Return space information for a space name.
//!
//! @param  vsP    - Pointer to the XrdOssVSInfo object to hold results.
//!                  It should be fully initialized (i.e. a new copy).
//! @param  sname  - Pointer to the space name. If the name starts with a
//!                  plus (e.g. "+public"), partition information is
//!                  returned, should it exist. If nil, space information for
//!                  all spaces is returned. See, XrdOssVS.hh for more info.
//! @param  updt   - When true, a space update occurrs prior to a query.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       StatVS(XrdOssVSInfo *vsP, const char *sname=0, int updt=0);

//-----------------------------------------------------------------------------
//! Return logical extended attributes associated with a path.
//!
//! @param  path   - Path in whose information is wanted.
//! @param  buff   - Pointer to the buffer to hold the information.
//! @param  blen   - Length of the buffer. This is updated with the actual
//!                  number of bytes placed in the buffer as in snprintf().
//! @param  envP   - Pointer to environmental information.
//!
//! @return "oss.cgroup=<name>&oss.type={'f'|'d'|'o'}&oss.used=<totbytes>
//!          &oss.mt=<mtime>&oss.ct=<ctime>&oss.at=<atime>&oss.u=*&oss.g=*
//!          &oss.fs={'w'|'r'}"
//!         Upon failure -errno or -osserr (see XrdOssError.hh) returned.
//-----------------------------------------------------------------------------

virtual int       StatXA(const char *path, char *buff, int &blen,
                         XrdOucEnv *envP=0);

//-----------------------------------------------------------------------------
//! Return export attributes associated with a path.
//!
//! @param  path   - Path in whose information is wanted.
//! @param  attr   - Reference to where the inforamation is to be stored.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       StatXP(const char *path, unsigned long long &attr,
                         XrdOucEnv  *envP=0);

//-----------------------------------------------------------------------------
//! Truncate a file.
//!
//! @param  path   - Pointer to the path of the file to be truncated.
//! @param  fsize  - The size that the file is to have.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Truncate(const char *path, unsigned long long fsize,
                           XrdOucEnv *envP=0)=0;

//-----------------------------------------------------------------------------
//! Remove a file.
//!
//! @param  path   - Pointer to the path of the file to be removed.
//! @param  opts   - Options:
//!                  XRDOSS_isMIG  - this is a migratable path.
//!                  XRDOSS_isPFN  - do not apply name2name to path.
//!                  XRDOSS_Online - remove only the online copy.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0)=0;

                  // Default Name-to-Name Methods

//-----------------------------------------------------------------------------
//! Translate logical name to physical name V1 (deprecated).
//!
//! @param  Path   - Path in whose information is wanted.
//! @param  buff   - Pointer to the buffer to hold the new path.
//! @param  blen   - Length of the buffer.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Lfn2Pfn(const char *Path, char *buff, int blen)
                         {if ((int)strlen(Path) >= blen) return -ENAMETOOLONG;
                          strcpy(buff, Path); return 0;
                         }

//-----------------------------------------------------------------------------
//! Translate logical name to physical name V2.
//!
//! @param  Path   - Path in whose information is wanted.
//! @param  buff   - Pointer to the buffer to hold the new path.
//! @param  blen   - Length of the buffer.
//! @param  rc     - Place where failure return code is to be returned:
//!                  -errno or -osserr (see XrdOssError.hh).
//!
//! @return Pointer to the translated path upon success or nil on failure.
//-----------------------------------------------------------------------------
virtual
const char       *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
                         { (void)buff; (void)blen; rc = 0; return Path;}

//-----------------------------------------------------------------------------
//! Constructor and Destructor.
//-----------------------------------------------------------------------------

                XrdOss() {}
virtual        ~XrdOss() {}
};

/******************************************************************************/
/*           S t o r a g e   S y s t e m   I n s t a n t i a t o r            */
/******************************************************************************/

//------------------------------------------------------------------------------
//! Get an instance of a configured XrdOss object.
//!
//! @param  native_oss -> object that would have been used as the storage
//!                       system. The object is not initialized (i.e., Init()
//!                       has not yet been called). This allows one to easily
//!                       wrap the native implementation or to completely
//!                       replace it, as needed.
//! @param  Logger     -> The message routing object to be used in conjunction
//!                       with an XrdSysError object for error messages.
//! @param  config_fn  -> The name of the config file.
//! @param  parms      -> Any parameters specified after the path on the
//!                       ofs.osslib directive. If there are no parameters, the
//!                       pointer may be zero.
//! @param  envP       -> **Version2 Only** pointer to environmental info.
//!                       This pointer may be nil if no such information exists.
//!
//! @return Success:   -> an instance of the XrdOss object to be used as the
//!                       underlying storage system.
//!         Failure:      Null pointer which causes initialization to fail.
//!
//! The object creation function must be declared as an extern "C" function
//! in the plug-in shared library as follows:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! The typedef that describes the XRdOssStatInfoInit external.
//------------------------------------------------------------------------------

typedef XrdOss *(*XrdOssGetStorageSystem_t) (XrdOss        *native_oss,
                                             XrdSysLogger  *Logger,
                                             const char    *config_fn,
                                             const char    *parms);

typedef XrdOss *(*XrdOssGetStorageSystem2_t)(XrdOss       *native_oss,
                                             XrdSysLogger *Logger,
                                             const char   *config_fn,
                                             const char   *parms,
                                             XrdOucEnv    *envP);

typedef XrdOssGetStorageSystem2_t XrdOssAddStorageSystem2_t;

/*!
    extern "C" XrdOss *XrdOssGetStorageSystem(XrdOss       *native_oss,
                                              XrdSysLogger *Logger,
                                              const char   *config_fn,
                                              const char   *parms);

    An alternate entry point may be defined in lieu of the previous entry point.
    The plug-in loader looks for this entry point first before reverting to the
    older version 1 entry point/ Version 2 differs in that an extra parameter,
    the environmental pointer, is passed. Note that this pointer is also
    supplied via the EnvInfo() method. This, many times, is not workable as
    environmental information is needed as initialization time.

    extern "C" XrdOss *XrdOssGetStorageSystem2(XrdOss       *native_oss,
                                               XrdSysLogger *Logger,
                                               const char   *config_fn,
                                               const char   *parms,
                                               XrdOucEnv    *envP);

    When pushing additional wrappers, the following entry point is called
    for each library that is stacked. The parameter, curr_oss is the pointer
    to the fully initialized oss plugin being wrapped. The function should
    return a pointer to the wrapping plug-in or nil upon failure.

    extern "C" XrdOss *XrdOssAddStorageSystem2(XrdOss       *curr_oss,
                                               XrdSysLogger *Logger,
                                               const char   *config_fn,
                                               const char   *parms,
                                               XrdOucEnv    *envP);
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOssGetStorageSystem,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif

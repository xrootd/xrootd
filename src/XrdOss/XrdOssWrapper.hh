#ifndef _XRDOSSWRAPPER_H
#define _XRDOSSWRAPPER_H
/******************************************************************************/
/*                                                                            */
/*                      X r d O s s W r a p p e r . h h                       */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOss/XrdOss.hh"

/******************************************************************************/
//! This class defines a wrapper around the three basic XrdOss classes that
//! defines the Oss interface. It is meant to be used by pushed XrdOss plugins
//! that wish to intercept certain XrdOss method for an underlying Oss plugin.
//! Inheriting this class and providing it the underlying wrapped object
//! allows the derived class to easily intercept certain methods while
//! allow non-intercepted methods to pass through.
/******************************************************************************/

/******************************************************************************/
/*                    C l a s s   X r d O s s W r a p D F                     */
/******************************************************************************/

//! This class wraps the object that handles directory as well as file
//! oriented requests. It is used by the derived class to wrap the object
//! obtained by calling newDir() or newFile() in class XrdOss.
  
class XrdOssWrapDF : public XrdOssDF
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

virtual int     Opendir(const char *path, XrdOucEnv &env)
                       {return wrapDF.Opendir(path, env);}

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

virtual int     Readdir(char *buff, int blen)
                       {return wrapDF.Readdir(buff, blen);}

//-----------------------------------------------------------------------------
//! Set the stat() buffer where stat information is to be placed corresponding
//! to the directory entry returned by Readdir().
//!
//! @param  Stat   - Pointer to stat structure to be used.
//!
//! @return 0 upon success or -ENOTSUP if not supported.
//!
//! @note This is a one-time call as stat structure is reused for each Readdir.
//-----------------------------------------------------------------------------

virtual int     StatRet(struct stat *Stat) {return wrapDF.StatRet(Stat);}

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

virtual int     Fchmod(mode_t mode) {return wrapDF.Fchmod(mode);}

//-----------------------------------------------------------------------------
//! Flush filesystem cached pages for this file (used for checksums).
//-----------------------------------------------------------------------------

virtual void    Flush() {wrapDF.Flush();}

//-----------------------------------------------------------------------------
//! Return state information for this file.
//!
//! @param  buf    - Pointer to the structure where info it to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fstat(struct stat *buf) {return wrapDF.Fstat(buf);}

//-----------------------------------------------------------------------------
//! Synchronize associated file with media (synchronous).
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fsync() {return wrapDF.Fsync();}

//-----------------------------------------------------------------------------
//! Synchronize associated file with media (asynchronous).
//!
//! @param  aiop   - Pointer to async I/O request object.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Fsync(XrdSfsAio *aiop) {return wrapDF.Fsync(aiop);}

//-----------------------------------------------------------------------------
//! Set the size of the associated file.
//!
//! @param  flen   - The new size of the file.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Ftruncate(unsigned long long flen)
                         {return wrapDF.Ftruncate(flen);}

//-----------------------------------------------------------------------------
//! Return the memory mapped characteristics of the file.
//!
//! @param  addr   - Pointer to where the memory mapped address is to be returned.
//!
//! @return If mapped, the size of the file is returned and it memory location
//!         is placed in addr. Otherwise, addr is set to zero and zero is
//!         returned. Note that zero length files cannot be memory mapped.
//-----------------------------------------------------------------------------

virtual off_t   getMmap(void **addr) {return wrapDF.getMmap(addr);}

//-----------------------------------------------------------------------------
//! Return file compression charectistics.
//!
//! @param  cxidp  - Pointer to where the compression algorithm name returned.
//!
//! @return If the file is compressed, the region size if returned. Otherwise,
//!         zero is returned (file not compressed).
//-----------------------------------------------------------------------------

virtual int     isCompressed(char *cxidp=0) {return wrapDF.isCompressed(cxidp);}

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
                    {return wrapDF.Open(path, Oflag, Mode, env);}

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

virtual ssize_t pgRead (void* buffer, off_t offset, size_t rdlen,
                        uint32_t* csvec, uint64_t opts)
                       {return wrapDF.pgRead(buffer,offset,rdlen,csvec,opts);}

//-----------------------------------------------------------------------------
//! Read file pages and checksums using asynchronous I/O.
//!
//! @param  aioparm - Pointer to async I/O object controlling the I/O.
//! @param  opts    - Processing options (see above).
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     pgRead (XrdSfsAio* aioparm, uint64_t opts)
                       {return wrapDF.pgRead(aioparm, opts);}

//-----------------------------------------------------------------------------
//! Write file pages into a file with corresponding checksums.
//!
//! @param  buffer  - pointer to buffer containing the bytes to write.
//! @param  offset  - The offset where the write is to start. It must be
//!                   page aligned.
//! @param  wrlen   - The number of bytes to write. If amount is not an
//!                   integral number of XrdSys::PageSize bytes, then this must
//!                   be the last write to the file at or above the offset.
//! @param  csvec   - A vector which contains the corresponding CRC32 checksum
//!                   for each page. It must be size to
//!                   wrlen/XrdSys::PageSize + (wrlen%XrdSys::PageSize != 0)
//! @param  opts    - Processing options (see above).
//!
//! @return >= 0      The number of bytes written upon success.
//!                   or -errno or -osserr upon failure. (see XrdOssError.hh).
//! @return  < 0      -errno or -osserr upon failure. (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen,
                        uint32_t* csvec, uint64_t opts)
                       {return wrapDF.pgWrite(buffer,offset,wrlen,csvec,opts);}

//-----------------------------------------------------------------------------
//! Write file pages and checksums using asynchronous I/O.
//!
//! @param  aioparm - Pointer to async I/O object controlling the I/O.
//! @param  opts    - Processing options (see above).
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     pgWrite(XrdSfsAio* aioparm, uint64_t opts)
                       {return wrapDF.pgWrite(aioparm, opts);}

//-----------------------------------------------------------------------------
//! Preread file blocks into the file system cache.
//!
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to pre-read.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t Read(off_t offset, size_t size)
                    {return wrapDF.Read(offset, size);}

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
                    {return wrapDF.Read(buffer, offset, size);}

//-----------------------------------------------------------------------------
//! Read file bytes using asynchronous I/O.
//!
//! @param  aiop    - Pointer to async I/O object controlling the I/O.
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Read(XrdSfsAio *aiop) {return wrapDF.Read(aiop);}

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
                       {return wrapDF.ReadRaw(buffer, offset, size);}

//-----------------------------------------------------------------------------
//! Read file bytes as directed by the read vector.
//!
//! @param  readV     pointer to the array of read requests.
//! @param  rdvcnt    the number of elements in readV.
//!
//! @return >=0       The numbe of bytes read.
//! @return < 0       -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t ReadV(XrdOucIOVec *readV, int rdvcnt)
                     {return wrapDF.ReadV(readV, rdvcnt);}

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
                     {return wrapDF.Write(buffer, offset, size);}

//-----------------------------------------------------------------------------
//! Write file bytes using asynchronous I/O.
//!
//! @param  aiop    - Pointer to async I/O object controlling the I/O.
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int     Write(XrdSfsAio *aiop) {return wrapDF.Write(aiop);}

//-----------------------------------------------------------------------------
//! Write file bytes as directed by the write vector.
//!
//! @param  writeV    pointer to the array of write requests.
//! @param  wrvcnt    the number of elements in writeV.
//!
//! @return >=0       The numbe of bytes read.
//! @return < 0       -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual ssize_t WriteV(XrdOucIOVec *writeV, int wrvcnt)
                      {return wrapDF.WriteV(writeV, wrvcnt);}

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

virtual int     Close(long long *retsz=0) {return wrapDF.Close(retsz);}

//-----------------------------------------------------------------------------
//! Return the underlying object type.
//!
//! @return Type of object.
//-----------------------------------------------------------------------------

uint16_t        DFType() {return wrapDF.DFType();}

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

virtual int     Fctl(int cmd, int alen, const char *args, char **resp=0)
                    {return wrapDF.Fctl(cmd, alen, args, resp);}

//-----------------------------------------------------------------------------
//! Obtain detailed error message text for the immediately preceeding 
//! directory or file error (see also XrdOssWrapper::getErrMsg()).
//!
//! @param  eText  - Where the message text is to be returned.
//!
//! @return True if message text is available, false otherwise.
//!
//! @note This method should be called using the same thread that encountered
//!       the error; otherwise, missleading error text may be returned.
//! @note Upon return, the internal error message text is cleared.
//-----------------------------------------------------------------------------

virtual bool    getErrMsg(std::string& eText) {return wrapDF.getErrMsg(eText);}

//-----------------------------------------------------------------------------
//! Return the underlying file descriptor.
//!
//! @return -1 if there is no file descriptor or a non-negative FD number.
//-----------------------------------------------------------------------------

virtual int     getFD() {return wrapDF.getFD();}

//-----------------------------------------------------------------------------
//! Return trace identifier associated with this object.
//!
//! @return Pointer to trace identifier
//-----------------------------------------------------------------------------
virtual
const char     *getTID() {return wrapDF.getTID();}

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//!
//! @param  df2Wrap- Reference to the newFile or newDir object being wrapped.
//!
//! @note: The object creator is responsible for deleting the df2Wrap object.
//!        The ref to this object is stored here and is accessible.
//-----------------------------------------------------------------------------

                XrdOssWrapDF(XrdOssDF &df2Wrap) : wrapDF(df2Wrap) {}

virtual        ~XrdOssWrapDF() {}


protected:

XrdOssDF   &wrapDF;  // Object being wrapped
};
  
/******************************************************************************/
/*                   C l a s s   X r d O s s W r a p p e r                    */
/******************************************************************************/
  
class XrdOssWrapper : public XrdOss
{
public:

//-----------------------------------------------------------------------------
//! Obtain a new director object to be used for future directory requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to a possibly wrapped XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------

virtual XrdOssDF     *newDir(const char *tident)
                            {return wrapPI.newDir(tident);}

//-----------------------------------------------------------------------------
//! Obtain a new file object to be used for a future file requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to a possibly wrapped XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------

virtual XrdOssDF     *newFile(const char *tident)
                             {return wrapPI.newFile(tident);}

//-----------------------------------------------------------------------------
//! Change file mode settings.
//!
//! @param  path   - Pointer to the path of the file in question.
//! @param  mode   - The new file mode setting.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Chmod(const char * path, mode_t mode, XrdOucEnv *envP=0)
                       {return wrapPI.Chmod(path, mode, envP);}

//-----------------------------------------------------------------------------
//! Notify storage system that a client has connected.
//!
//! @param  env    - Reference to environmental information.
//-----------------------------------------------------------------------------

virtual void      Connect(XrdOucEnv &env) {wrapPI.Connect(env);}

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

virtual int       Create(const char *tid, const char *path, mode_t mode,
                         XrdOucEnv &env, int opts=0)
                        {return wrapPI.Create(tid, path, mode, env, opts);}

//-----------------------------------------------------------------------------
//! Notify storage system that a client has disconnected.
//!
//! @param  env    - Reference to environmental information.
//-----------------------------------------------------------------------------

virtual void      Disc(XrdOucEnv &env) {wrapPI.Disc(env);}

//-----------------------------------------------------------------------------
//! Notify storage system of initialization information (deprecated).
//!
//! @param  envP   - Pointer to environmental information.
//-----------------------------------------------------------------------------

virtual void      EnvInfo(XrdOucEnv *envP) {wrapPI.EnvInfo(envP);}

//-----------------------------------------------------------------------------
//! Return storage system features.
//!
//! @return Storage system features (see XRDOSS_HASxxx flags).
//-----------------------------------------------------------------------------

virtual uint64_t  Features() {return wrapPI.Features();}

//-----------------------------------------------------------------------------
//! Obtain detailed error message text for the immediately preceeding error
//! returned by any method in this class.
//!
//! @param  eText  - Where the message text is to be returned.
//!
//! @return True if message text is available, false otherwise.
//!
//! @note This method should be called using the same thread that encountered
//!       the error; otherwise, missleading error text may be returned.
//! @note Upon return, the internal error message text is cleared.
//-----------------------------------------------------------------------------

virtual bool      getErrMsg(std::string& eText)
                           {return wrapPI.getErrMsg(eText);}

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

virtual int       FSctl(int cmd, int alen, const char *args, char **resp=0)
                       {return wrapPI.FSctl(cmd, alen, args, resp);}

//-----------------------------------------------------------------------------
//! Initialize the storage system V1 (deprecated).
//!
//! @param  lp     - Pointer to the message logging object.
//! @param  cfn    - Pointer to the configuration file.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Init(XrdSysLogger *lp, const char *cfn)
                      {return wrapPI.Init(lp, cfn);}

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
                      {return wrapPI.Init(lp, cfn, envP);}

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
                        XrdOucEnv  *envP=0)
                       {return wrapPI.Mkdir(path, mode, mkpath, envP);}

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
                        const char *cgName, const char *anchor=0)
                       {return wrapPI.Reloc(tident,path,cgName,anchor);}

//-----------------------------------------------------------------------------
//! Remove a directory.
//!
//! @param  path   - Pointer to the path of the directory to be removed.
//! @param  Opts   - The processing options:
//!                  XRDOSS_Online   - only remove online copy
//!                  XRDOSS_isPFN    - path is already translated.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0)
                        {return wrapPI.Remdir(path, Opts, envP);}

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
                         XrdOucEnv  *oEnvP=0, XrdOucEnv *nEnvP=0)
                        {return wrapPI.Rename(oPath, nPath, oEnvP, nEnvP);}

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
                       int opts=0, XrdOucEnv *envP=0)
                      {return wrapPI.Stat(path, buff, opts, envP);}

//-----------------------------------------------------------------------------
//! Return statistics.
//!
//! @param  buff   - Pointer to the buffer to hold statistics.
//! @param  blen   - Length of the buffer.
//!
//! @return The number of bytes placed in the buffer excluding null byte.
//-----------------------------------------------------------------------------

virtual int       Stats(char *buff, int blen)
                       {return wrapPI.Stats(buff, blen);}

//-----------------------------------------------------------------------------
//! Return filesystem physical space information associated with a path.
//!
//! @param  path   - Path in the partition in question.
//! @param  buff   - Pointer to the buffer to hold the information.
//! @param  blen   - Length of the buffer. This is updated with the actual
//!                  number of bytes placed in the buffer as in snprintf().
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
                         XrdOucEnv  *envP=0)
                        {return wrapPI.StatFS(path, buff, blen, envP);}

//-----------------------------------------------------------------------------
//! Return filesystem physical space information associated with a space name.
//!
//! @param  env    - Ref to environmental information. If the environment
//!                  has the key oss.cgroup defined, the associated value is
//!                  used as the space name and the path is ignored.
//! @param  path   - Path in the name space in question. The space name
//!                  associated with gthe path is used unless overridden.
//! @param  buff   - Pointer to the buffer to hold the information.
//! @param  blen   - Length of the buffer. This is updated with the actual
//!                  number of bytes placed in the buffer as in snprintf().
//!
//! @return "oss.cgroup=<name>&oss.space=<totbytes>&oss.free=<freebytes>
//!          &oss.maxf=<maxcontigbytes>&oss.used=<bytesused>
//!          &oss.quota=<quotabytes>" in buff upon success.
//!         Upon failure -errno or -osserr (see XrdOssError.hh) returned.
//-----------------------------------------------------------------------------

virtual int       StatLS(XrdOucEnv &env, const char *path,
                         char *buff, int &blen)
                        {return wrapPI.StatLS(env, path, buff, blen);}

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

virtual int       StatPF(const char *path, struct stat *buff, int opts)
                        {return wrapPI.StatPF(path, buff, opts);}

virtual int       StatPF(const char *path, struct stat *buff)
                        {return wrapPI.StatPF(path, buff);}

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

virtual int       StatVS(XrdOssVSInfo *vsP, const char *sname=0, int updt=0)
                        {return wrapPI.StatVS(vsP, sname, updt);}

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
                         XrdOucEnv *envP=0)
                        {return wrapPI.StatXA(path, buff, blen, envP);}

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
                         XrdOucEnv  *envP=0)
                        {return wrapPI.StatXP(path, attr, envP);}

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
                           XrdOucEnv *envP=0)
                          {return wrapPI.Truncate(path, fsize, envP);}

//-----------------------------------------------------------------------------
//! Remove a file.
//!
//! @param  path   - Pointer to the path of the file to be removed.
//! @param  Opts   - Options:
//!                  XRDOSS_isMIG  - this is a migratable path.
//!                  XRDOSS_isPFN  - do not apply name2name to path.
//!                  XRDOSS_Online - remove only the online copy.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0)
                        {return wrapPI.Unlink(path, Opts, envP);}

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
                         {return wrapPI.Lfn2Pfn(Path, buff, blen);}

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
                         {return wrapPI.Lfn2Pfn(Path, buff, blen, rc);}

//-----------------------------------------------------------------------------
//! Constructor and Destructor.
//!
//! @param  ossRef - Reference to the Oss object being wrapped.
//!
//! @note: The object creator is responsible for deleting the ossRef object.
//!        The ref to this object is stored here and is accessible.
//-----------------------------------------------------------------------------

                XrdOssWrapper(XrdOss &ossRef) : wrapPI(ossRef) {}
virtual        ~XrdOssWrapper() {}

protected:

XrdOss &wrapPI;
};
#endif

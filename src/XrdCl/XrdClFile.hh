//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_FILE_HH__
#define __XRD_CL_FILE_HH__

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClOptional.hh"
#include "XrdOuc/XrdOucCompiler.hh"
#include <cstdint>
#include <string>
#include <vector>
#include <sys/uio.h>

namespace XrdCl
{
  class FileStateHandler;
  class FilePlugIn;

  //----------------------------------------------------------------------------
  //! A file
  //----------------------------------------------------------------------------
  class File
  {
    public:

      enum VirtRedirect
      {
        EnableVirtRedirect,
        DisableVirtRedirect
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      File( bool enablePlugIns = true );

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      File( VirtRedirect virtRedirect, bool enablePlugIns = true );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~File();

      //------------------------------------------------------------------------
      //! Open the file pointed to by the given URL - async
      //!
      //! @param url     url of the file to be opened
      //! @param flags   OpenFlags::Flags
      //! @param mode    Access::Mode for new files, 0 otherwise
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Open( const std::string &url,
                         OpenFlags::Flags   flags,
                         Access::Mode       mode,
                         ResponseHandler   *handler,
                         uint16_t           timeout  = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Open the file pointed to by the given URL - sync
      //!
      //! @param url     url of the file to be opened
      //! @param flags   OpenFlags::Flags
      //! @param mode    Access::Mode for new files, 0 otherwise
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Open( const std::string &url,
                         OpenFlags::Flags   flags,
                         Access::Mode       mode    = Access::None,
                         uint16_t           timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Close the file - async
      //!
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( ResponseHandler *handler,
                          uint16_t         timeout = 0 )
                          XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Close the file - sync
      //!
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( uint16_t timeout = 0 ) XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Obtain status information for this file - async
      //!
      //! @param force   do not use the cached information, force re-stating
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a StatInfo object
      //!                if the procedure is successful
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Stat( bool             force,
                         ResponseHandler *handler,
                         uint16_t         timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Obtain status information for this file - sync
      //!
      //! @param force   do not use the cached information, force re-stating
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Stat( bool       force,
                         StatInfo *&response,
                         uint16_t   timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;


      //------------------------------------------------------------------------
      //! Read a data chunk at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be read
      //! @param buffer  a pointer to a buffer big enough to hold the data
      //!                or 0 if the buffer should be allocated by the system
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a ChunkInfo object if
      //!                the procedure was successful
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Read( uint64_t         offset,
                         uint32_t         size,
                         void            *buffer,
                         ResponseHandler *handler,
                         uint16_t         timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Read a data chunk at a given offset - sync
      //!
      //! @param offset    offset from the beginning of the file
      //! @param size      number of bytes to be read
      //! @param buffer    a pointer to a buffer big enough to hold the data
      //! @param bytesRead number of bytes actually read
      //! @param timeout   timeout value, if 0 the environment default will be
      //!                  used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Read( uint64_t  offset,
                         uint32_t  size,
                         void     *buffer,
                         uint32_t &bytesRead,
                         uint16_t  timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Read number of pages at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    buffer size, at least 1 page big (4KB)
      //! @param buffer  a pointer to a buffer big enough to hold the data
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a PageInfo object if
      //!                the procedure was successful
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus PgRead( uint64_t         offset,
                           uint32_t         size,
                           void            *buffer,
                           ResponseHandler *handler,
                           uint16_t         timeout = 0 )
                           XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Read a data chunk at a given offset - sync
      //!
      //! @param offset    offset from the beginning of the file
      //! @param size    buffer size, at least 1 page big (4KB)
      //! @param buffer    a pointer to a buffer big enough to hold the data
      //! @param bytesRead number of bytes actually read
      //! @param cksums    crc32c checksum for each read 4KB page
      //! @param timeout   timeout value, if 0 the environment default will be
      //!                  used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus PgRead( uint64_t               offset,
                           uint32_t               size,
                           void                  *buffer,
                           std::vector<uint32_t> &cksums,
                           uint32_t              &bytesRead,
                           uint16_t               timeout = 0 )
                           XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write a data chunk at a given offset - async
      //! The call interprets and returns the server response, which may be
      //! either a success or a failure, it does not contain the number
      //! of bytes that were actually written.
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param buffer  a pointer to the buffer holding the data to be written
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t         offset,
                          uint32_t         size,
                          const void      *buffer,
                          ResponseHandler *handler,
                          uint16_t         timeout = 0 )
                          XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write a data chunk at a given offset - sync
      //! The call interprets and returns the server response, which may be
      //! either a success or a failure, it does not contain the number
      //! of bytes that were actually written.
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param buffer  a pointer to the buffer holding the data to be
      //!                written
      //! @param timeout timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t    offset,
                          uint32_t    size,
                          const void *buffer,
                          uint16_t    timeout = 0 )
                          XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write a data chunk at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param buffer  r-value reference to Buffer object, in this case XrdCl
      //!                runtime takes ownership of the buffer
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t          offset,
                          Buffer          &&buffer,
                          ResponseHandler  *handler,
                          uint16_t          timeout = 0 );

      //------------------------------------------------------------------------
      //! Write a data chunk at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param buffer  r-value reference to Buffer object, in this case XrdCl
      //!                runtime takes ownership of the buffer
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t          offset,
                          Buffer          &&buffer,
                          uint16_t          timeout = 0 );

      //------------------------------------------------------------------------
      //! Write a data from a given file descriptor at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param fdoff   offset of the data to be written from the file descriptor
      //!                (optional, if not provided will copy data from the file
      //!                descriptor at the current cursor position)
      //! @param fd      file descriptor open for reading
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t            offset,
                          uint32_t            size,
                          Optional<uint64_t>  fdoff,
                          int                 fd,
                          ResponseHandler    *handler,
                          uint16_t            timeout = 0 );

      //------------------------------------------------------------------------
      //! Write a data from a given file descriptor at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param fdoff   offset of the data to be written from the file descriptor
      //!                (optional, if not provided will copy data from the file
      //!                descriptor at the current cursor position)
      //! @param fd      file descriptor open for reading
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t            offset,
                          uint32_t            size,
                          Optional<uint64_t>  fdoff,
                          int                 fd,
                          uint16_t            timeout = 0 );

      //------------------------------------------------------------------------
      //! Write number of pages at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    buffer size
      //! @param buffer  a pointer to a buffer holding data pages
      //! @param cksums  the crc32c checksums for each 4KB page
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus PgWrite( uint64_t               offset,
                            uint32_t               size,
                            const void            *buffer,
                            std::vector<uint32_t> &cksums,
                            ResponseHandler       *handler,
                            uint16_t               timeout = 0 )
                            XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write number of pages at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    buffer size
      //! @param buffer  a pointer to a buffer holding data pages
      //! @param cksums  the crc32c checksums for each 4KB page
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus PgWrite( uint64_t               offset,
                            uint32_t               size,
                            const void            *buffer,
                            std::vector<uint32_t> &cksums,
                            uint16_t               timeout = 0 )
                            XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Commit all pending disk writes - async
      //!
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Sync( ResponseHandler *handler,
                         uint16_t         timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;


      //------------------------------------------------------------------------
      //! Commit all pending disk writes - sync
      //!
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Sync( uint16_t timeout = 0 ) XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Truncate the file to a particular size - async
      //!
      //! @param size    desired size of the file
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Truncate( uint64_t         size,
                             ResponseHandler *handler,
                             uint16_t         timeout = 0 )
                             XRD_WARN_UNUSED_RESULT;


      //------------------------------------------------------------------------
      //! Truncate the file to a particular size - sync
      //!
      //! @param size    desired size of the file
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Truncate( uint64_t size,
                             uint16_t timeout = 0 )
                             XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Read scattered data chunks in one operation - async
      //!
      //! @param chunks    list of the chunks to be read and buffers to put
      //!                  the data in. The default maximum chunk size is
      //!                  2097136 bytes and the default maximum number
      //!                  of chunks per request is 1024. The server
      //!                  may be queried using FileSystem::Query for the
      //!                  actual settings.
      //! @param buffer    if zero the buffer pointers in the chunk list
      //!                  will be used, otherwise it needs to point to a
      //!                  buffer big enough to hold the requested data
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus VectorRead( const ChunkList &chunks,
                               void            *buffer,
                               ResponseHandler *handler,
                               uint16_t         timeout = 0 )
                               XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Read scattered data chunks in one operation - sync
      //!
      //! @param chunks    list of the chunks to be read and buffers to put
      //!                  the data in. The default maximum chunk size is
      //!                  2097136 bytes and the default maximum number
      //!                  of chunks per request is 1024. The server
      //!                  may be queried using FileSystem::Query for the
      //!                  actual settings.
      //! @param buffer    if zero the buffer pointers in the chunk list
      //!                  will be used, otherwise it needs to point to a
      //!                  buffer big enough to hold the requested data
      //! @param vReadInfo buffer size and chunk information
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus VectorRead( const ChunkList  &chunks,
                               void             *buffer,
                               VectorReadInfo  *&vReadInfo,
                               uint16_t          timeout = 0 )
                               XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write scattered data chunks in one operation - async
      //!
      //! @param chunks    list of the chunks to be written.
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus VectorWrite( const ChunkList &chunks,
                                ResponseHandler *handler,
                                uint16_t         timeout = 0 )
                                XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write scattered data chunks in one operation - sync
      //!
      //! @param chunks    list of the chunks to be written.
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus VectorWrite( const ChunkList  &chunks,
                               uint16_t          timeout = 0 )
                               XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Write scattered buffers in one operation - async
      //!
      //! @param offset    offset from the beginning of the file
      //! @param iov       list of the buffers to be written
      //! @param iovcnt    number of buffers
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus WriteV( uint64_t            offset,
                           const struct iovec *iov,
                           int                 iovcnt,
                           ResponseHandler    *handler,
                           uint16_t            timeout = 0 );

      //------------------------------------------------------------------------
      //! Write scattered buffers in one operation - sync
      //!
      //! @param offset    offset from the beginning of the file
      //! @param iov       list of the buffers to
      //! @param iovcnt    number of buffers
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus WriteV( uint64_t            offset,
                           const struct iovec *iov,
                           int                 iovcnt,
                           uint16_t            timeout = 0 );

      //------------------------------------------------------------------------
      //! Read data into scattered buffers in one operation - async
      //!
      //! @param offset    offset from the beginning of the file
      //! @param iov       list of the buffers to be written
      //! @param iovcnt    number of buffers
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ReadV( uint64_t         offset,
                          struct iovec    *iov,
                          int              iovcnt,
                          ResponseHandler *handler,
                          uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Read data into scattered buffers in one operation - sync
      //!
      //! @param offset    offset from the beginning of the file
      //! @param iov       list of the buffers to be written
      //! @param iovcnt    number of buffers
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ReadV( uint64_t      offset,
                          struct iovec *iov,
                          int           iovcnt,
                          uint32_t     &bytesRead,
                          uint16_t      timeout = 0 );

      //------------------------------------------------------------------------
      //! Performs a custom operation on an open file, server implementation
      //! dependent - async
      //!
      //! @param arg       query argument
      //! @param handler   handler to be notified when the response arrives,
      //!                  the response parameter will hold a Buffer object
      //!                  if the procedure is successful
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Fcntl( const Buffer    &arg,
                          ResponseHandler *handler,
                          uint16_t         timeout = 0 )
                          XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Performs a custom operation on an open file, server implementation
      //! dependent - sync
      //!
      //! @param arg       query argument
      //! @param response  the response (to be deleted by the user)
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Fcntl( const Buffer     &arg,
                          Buffer          *&response,
                          uint16_t          timeout = 0 )
                          XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Get access token to a file - async
      //!
      //! @param handler   handler to be notified when the response arrives,
      //!                  the response parameter will hold a Buffer object
      //!                  if the procedure is successful
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Visa( ResponseHandler *handler,
                         uint16_t         timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Get access token to a file - sync
      //!
      //! @param visa      the access token (to be deleted by the user)
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Visa( Buffer   *&visa,
                         uint16_t   timeout = 0 )
                         XRD_WARN_UNUSED_RESULT;

      //------------------------------------------------------------------------
      //! Set extended attributes - async
      //!
      //! @param attrs   : list of extended attributes to set
      //! @param handler : handler to be notified when the response arrives,
      //!                  the response parameter will hold a std::vector of
      //!                  XAttrStatus objects
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus SetXAttr( const std::vector<xattr_t>  &attrs,
                             ResponseHandler             *handler,
                             uint16_t                     timeout = 0 );

      //------------------------------------------------------------------------
      //! Set extended attributes - sync
      //!
      //! @param attrs   : list of extended attributes to set
      //! @param result  : result of the operation
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus SetXAttr( const std::vector<xattr_t>  &attrs,
                             std::vector<XAttrStatus>    &result,
                             uint16_t                     timeout = 0 );

      //------------------------------------------------------------------------
      //! Get extended attributes - async
      //!
      //! @param attrs   : list of extended attributes to get
      //! @param handler : handler to be notified when the response arrives,
      //!                  the response parameter will hold a std::vector of
      //!                  XAttr objects
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus GetXAttr( const std::vector<std::string>  &attrs,
                             ResponseHandler                 *handler,
                             uint16_t                         timeout = 0 );

      //------------------------------------------------------------------------
      //! Get extended attributes - sync
      //!
      //! @param attrs   : list of extended attributes to get
      //! @param result  : result of the operation
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus GetXAttr( const std::vector<std::string>  &attrs,
                             std::vector<XAttr>              &result,
                             uint16_t                         timeout = 0 );

      //------------------------------------------------------------------------
      //! Delete extended attributes - async
      //!
      //! @param attrs   : list of extended attributes to set
      //! @param handler : handler to be notified when the response arrives,
      //!                  the response parameter will hold a std::vector of
      //!                  XAttrStatus objects
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus DelXAttr( const std::vector<std::string>  &attrs,
                             ResponseHandler                 *handler,
                             uint16_t                         timeout = 0 );

      //------------------------------------------------------------------------
      //! Delete extended attributes - sync
      //!
      //! @param attrs   : list of extended attributes to set
      //! @param result  : result of the operation
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus DelXAttr( const std::vector<std::string>  &attrs,
                             std::vector<XAttrStatus>        &result,
                             uint16_t                         timeout = 0 );

      //------------------------------------------------------------------------
      //! List extended attributes - async
      //!
      //! @param handler : handler to be notified when the response arrives,
      //!                  the response parameter will hold a std::vector of
      //!                  XAttr objects
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ListXAttr( ResponseHandler           *handler,
                              uint16_t                   timeout = 0 );

      //------------------------------------------------------------------------
      //! List extended attributes - sync
      //!
      //! @param result  : result of the operation
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ListXAttr( std::vector<XAttr>  &result,
                              uint16_t             timeout = 0 );

      //------------------------------------------------------------------------
      //! Try different data server
      //!
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus TryOtherServer( uint16_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Check if the file is open
      //------------------------------------------------------------------------
      bool IsOpen() const;

      //------------------------------------------------------------------------
      //! Check if the file is using an encrypted connection
      //------------------------------------------------------------------------
      bool IsSecure() const;

      //------------------------------------------------------------------------
      //! Set file property
      //!
      //! File properties:
      //! ReadRecovery     [true/false] - enable/disable read recovery
      //! WriteRecovery    [true/false] - enable/disable write recovery
      //! FollowRedirects  [true/false] - enable/disable following redirections
      //! BundledClose     [true/false] - enable/disable bundled close
      //------------------------------------------------------------------------
      bool SetProperty( const std::string &name, const std::string &value );

      //------------------------------------------------------------------------
      //! Get file property
      //!
      //! @see File::SetProperty for property list
      //!
      //! Read-only properties:
      //! DataServer [string] - the data server the file is accessed at
      //! LastURL    [string] - final file URL with all the cgi information
      //------------------------------------------------------------------------
      bool GetProperty( const std::string &name, std::string &value ) const;

    private:

      template<bool HasHndl>
      friend class CheckpointImpl;

      template<bool HasHndl>
      friend class ChkptWrtImpl;

      template <bool HasHndl>
      friend class ChkptWrtVImpl;

      //------------------------------------------------------------------------
      //! Create a checkpoint - async
      //!
      //! @param handler : handler to be notified when the response arrives,
      //!                  the response parameter will hold a std::vector of
      //!                  XAttr objects
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Checkpoint( kXR_char                  code,
                               ResponseHandler          *handler,
                               uint16_t                  timeout = 0 );

      //------------------------------------------------------------------------
      //! Checkpointed write - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param buffer  a pointer to the buffer holding the data to be written
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ChkptWrt( uint64_t         offset,
                             uint32_t         size,
                             const void      *buffer,
                             ResponseHandler *handler,
                             uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Checkpointed WriteV - async
      //!
      //! @param offset    offset from the beginning of the file
      //! @param iov       list of the buffers to be written
      //! @param iovcnt    number of buffers
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ChkptWrtV( uint64_t            offset,
                              const struct iovec *iov,
                              int                 iovcnt,
                              ResponseHandler    *handler,
                              uint16_t            timeout = 0 );

      FileStateHandler *pStateHandler;
      FilePlugIn       *pPlugIn;
      bool              pEnablePlugIns;
  };
}

#endif // __XRD_CL_FILE_HH__

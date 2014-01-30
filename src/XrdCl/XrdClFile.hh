//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef __XRD_CL_FILE_HH__
#define __XRD_CL_FILE_HH__

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include <stdint.h>
#include <string>
#include <vector>

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
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      File( bool enablePlugIns = true );

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
                         uint16_t           timeout  = 0 );

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
                         uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Close the file - async
      //!
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( ResponseHandler *handler,
                          uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Close the file - sync
      //!
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( uint16_t timeout = 0 );

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
                         uint16_t         timeout = 0 );

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
                         uint16_t   timeout = 0 );


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
                         uint16_t         timeout = 0 );

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
                         uint16_t  timeout = 0 );

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
                          uint16_t         timeout = 0 );

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
                          uint16_t    timeout = 0 );

      //------------------------------------------------------------------------
      //! Commit all pending disk writes - async
      //!
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Sync( ResponseHandler *handler,
                         uint16_t         timeout = 0 );


      //------------------------------------------------------------------------
      //! Commit all pending disk writes - sync
      //!
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Sync( uint16_t timeout = 0 );

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
                             uint16_t         timeout = 0 );


      //------------------------------------------------------------------------
      //! Truncate the file to a particular size - sync
      //!
      //! @param size    desired size of the file
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Truncate( uint64_t size, uint16_t timeout = 0 );

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
                               uint16_t         timeout = 0 );

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
                               uint16_t          timeout = 0 );

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
                          uint16_t         timeout = 0 );

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
                          uint16_t          timeout = 0 );

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
                         uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Get access token to a file - sync
      //!
      //! @param visa      the access token (to be deleted by the user)
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Visa( Buffer   *&visa,
                         uint16_t   timeout = 0 );

      //------------------------------------------------------------------------
      //! Check if the file is open
      //------------------------------------------------------------------------
      bool IsOpen() const;

      //------------------------------------------------------------------------
      //! Enable/disable state recovery procedures while the file is open for
      //! reading
      //------------------------------------------------------------------------
      void EnableReadRecovery( bool enable = true );

      //------------------------------------------------------------------------
      //! Enable/disable state recovery procedures while the file is open for
      //! writing or read/write
      //------------------------------------------------------------------------
      void EnableWriteRecovery( bool enable = true );

      //------------------------------------------------------------------------
      //! Get the data server the file is accessed at
      //------------------------------------------------------------------------
      std::string GetDataServer() const;

      //------------------------------------------------------------------------
      //! Get final url with all the cgi information
      //------------------------------------------------------------------------
      URL GetLastURL() const;

    private:
      FileStateHandler *pStateHandler;
      FilePlugIn       *pPlugIn;
      bool              pEnablePlugIns;
  };
}

#endif // __XRD_CL_FILE_HH__

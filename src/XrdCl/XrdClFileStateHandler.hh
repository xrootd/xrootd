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

#ifndef __XRD_CL_FILE_STATE_HANDLER_HH__
#define __XRD_CL_FILE_STATE_HANDLER_HH__

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClLocalFileHandler.hh"
#include "XrdCl/XrdClOptional.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysPageSize.hh"

#include <list>
#include <set>
#include <vector>

#include <sys/time.h>
#include <sys/uio.h>
#include <cstdint>

namespace
{
  class PgReadHandler;
  class PgReadRetryHandler;
  class PgReadSubstitutionHandler;
  class OpenHandler;
}

namespace XrdCl
{
  class Message;
  class EcHandler;

  //----------------------------------------------------------------------------
  //! PgRead flags
  //----------------------------------------------------------------------------
  struct PgReadFlags
  {
      //------------------------------------------------------------------------
      //! PgRead flags
      //------------------------------------------------------------------------
      enum Flags
      {
        None  = 0,                      //< Nothing
        Retry = XrdProto::kXR_pgRetry  //< Retry reading currupted page

      };
  };
  XRDOUC_ENUM_OPERATORS( PgReadFlags::Flags )

  //----------------------------------------------------------------------------
  //! Handle the stateful operations
  //----------------------------------------------------------------------------
  class FileStateHandler
  {
      friend class ::PgReadHandler;
      friend class ::PgReadRetryHandler;
      friend class ::PgReadSubstitutionHandler;
      friend class ::OpenHandler;

    public:
      //------------------------------------------------------------------------
      //! State of the file
      //------------------------------------------------------------------------
      enum FileStatus
      {
        Closed,           //!< The file is closed
        Opened,           //!< Opening has succeeded
        Error,            //!< Opening has failed
        Recovering,       //!< Recovering from an error
        OpenInProgress,   //!< Opening is in progress
        CloseInProgress   //!< Closing operation is in progress
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      FileStateHandler( FilePlugIn *& plugin );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param useVirtRedirector if true Metalink files will be treated
      //!                          as a VirtualRedirectors
      //------------------------------------------------------------------------
      FileStateHandler( bool useVirtRedirector, FilePlugIn *& plugin );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~FileStateHandler();

      //------------------------------------------------------------------------
      //! Open the file pointed to by the given URL
      //!
      //! @param url     url of the file to be opened
      //! @param flags   OpenFlags::Flags
      //! @param mode    Access::Mode for new files, 0 otherwise
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Open( std::shared_ptr<FileStateHandler> &self,
                                const std::string                 &url,
                                uint16_t                           flags,
                                uint16_t                           mode,
                                ResponseHandler                   *handler,
                                uint16_t                           timeout  = 0 );

      //------------------------------------------------------------------------
      //! Close the file object
      //!
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Close( std::shared_ptr<FileStateHandler> &self,
                                 ResponseHandler                   *handler,
                                 uint16_t                           timeout = 0 );

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
      static XRootDStatus Stat( std::shared_ptr<FileStateHandler> &self,
                                bool                               force,
                                ResponseHandler                   *handler,
                                uint16_t                           timeout = 0 );


      //------------------------------------------------------------------------
      //! Read a data chunk at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be read
      //! @param buffer  a pointer to a buffer big enough to hold the data
      //!                or 0 if the buffer should be allocated by the system
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a buffer object if
      //!                the procedure was successful, if a preallocated
      //!                buffer was specified then the buffer object will
      //!                "wrap" this buffer
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Read( std::shared_ptr<FileStateHandler> &self,
                                uint64_t                           offset,
                                uint32_t                           size,
                                void                              *buffer,
                                ResponseHandler                   *handler,
                                uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Read data pages at a given offset
      //!
      //! @param offset  : offset from the beginning of the file (Note: has to
      //!                  4KB aligned)
      //! @param size    : buffer size
      //! @param buffer  : a pointer to buffer big enough to hold the data
      //! @param handler : handler to be notified when the response arrives, the
      //!                  response parameter will hold a PgReadInfo object if
      //!                  the procedure was successful
      //! @param timeout : timeout value, if 0 environment default will be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus PgRead( std::shared_ptr<FileStateHandler> &self,
                                  uint64_t                           offset,
                                  uint32_t                           size,
                                  void                              *buffer,
                                  ResponseHandler                   *handler,
                                  uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Retry reading one page of data at a given offset
      //!
      //! @param offset  : offset from the beginning of the file (Note: has to
      //!                  4KB aligned)
      //! @param size    : buffer size
      //! @param buffer  : a pointer to buffer big enough to hold the data
      //! @param handler : handler to be notified when the response arrives
      //! @param timeout : timeout value, if 0 environment default will be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus PgReadRetry( std::shared_ptr<FileStateHandler> &self,
                                       uint64_t                           offset,
                                       uint32_t                           size,
                                       size_t                             pgnb,
                                       void                              *buffer,
                                       PgReadHandler                     *handler,
                                       uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Read data pages at a given offset (actual implementation)
      //!
      //! @param offset  : offset from the beginning of the file (Note: has to
      //!                  4KB aligned)
      //! @param size    : buffer size
      //! @param buffer  : a pointer to buffer big enough to hold the data
      //! @param flags   : PgRead flags
      //! @param handler : handler to be notified when the response arrives, the
      //!                  response parameter will hold a PgReadInfo object if
      //!                  the procedure was successful
      //! @param timeout : timeout value, if 0 environment default will be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus PgReadImpl( std::shared_ptr<FileStateHandler> &self,
                                      uint64_t                           offset,
                                      uint32_t                           size,
                                      void                              *buffer,
                                      uint16_t                           flags,
                                      ResponseHandler                   *handler,
                                      uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Write a data chunk at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param buffer  a pointer to the buffer holding the data to be written
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Write( std::shared_ptr<FileStateHandler> &self,
                                 uint64_t                           offset,
                                 uint32_t                           size,
                                 const void                        *buffer,
                                 ResponseHandler                   *handler,
                                 uint16_t                           timeout = 0 );

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
      static XRootDStatus Write( std::shared_ptr<FileStateHandler> &self,
                                 uint64_t                           offset,
                                 Buffer                           &&buffer,
                                 ResponseHandler                   *handler,
                                 uint16_t                           timeout = 0 );

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
      static XRootDStatus Write( std::shared_ptr<FileStateHandler> &self,
                                 uint64_t                           offset,
                                 uint32_t                           size,
                                 Optional<uint64_t>                 fdoff,
                                 int                                fd,
                                 ResponseHandler                   *handler,
                                 uint16_t                           timeout = 0 );

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
      static XRootDStatus PgWrite( std::shared_ptr<FileStateHandler> &self,
                                   uint64_t                           offset,
                                   uint32_t                           size,
                                   const void                        *buffer,
                                   std::vector<uint32_t>             &cksums,
                                   ResponseHandler                   *handler,
                                   uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Write number of pages at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    buffer size
      //! @param buffer  a pointer to a buffer holding data pages
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus PgWriteRetry( std::shared_ptr<FileStateHandler> &self,
                                        uint64_t                           offset,
                                        uint32_t                           size,
                                        const void                        *buffer,
                                        uint32_t                           digest,
                                        ResponseHandler                   *handler,
                                        uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Write number of pages at a given offset - async
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    buffer size
      //! @param buffer  a pointer to a buffer holding data pages
      //! @param cksums  the crc32c checksums for each 4KB page
      //! @param flags   PgWrite flags
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus PgWriteImpl( std::shared_ptr<FileStateHandler> &self,
                                       uint64_t                           offset,
                                       uint32_t                           size,
                                       const void                        *buffer,
                                       std::vector<uint32_t>             &cksums,
                                       kXR_char                           flags,
                                       ResponseHandler                   *handler,
                                       uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Commit all pending disk writes - async
      //!
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Sync( std::shared_ptr<FileStateHandler> &self,
                                ResponseHandler                   *handler,
                                uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Truncate the file to a particular size - async
      //!
      //! @param size    desired size of the file
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Truncate( std::shared_ptr<FileStateHandler> &self,
                                    uint64_t                           size,
                                    ResponseHandler                   *handler,
                                    uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Read scattered data chunks in one operation - async
      //!
      //! @param chunks    list of the chunks to be read
      //! @param buffer    a pointer to a buffer big enough to hold the data
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus VectorRead( std::shared_ptr<FileStateHandler> &self,
                                      const ChunkList                   &chunks,
                                      void                              *buffer,
                                      ResponseHandler                   *handler,
                                      uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Write scattered data chunks in one operation - async
      //!
      //! @param chunks    list of the chunks to be read
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus VectorWrite( std::shared_ptr<FileStateHandler> &self,
                                       const ChunkList                   &chunks,
                                       ResponseHandler                   *handler,
                                       uint16_t                           timeout = 0 );

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
      static XRootDStatus WriteV( std::shared_ptr<FileStateHandler> &self,
                                  uint64_t                           offset,
                                  const struct iovec                *iov,
                                  int                                iovcnt,
                                  ResponseHandler                   *handler,
                                  uint16_t                           timeout = 0 );

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
      static XRootDStatus ReadV( std::shared_ptr<FileStateHandler> &self,
                                 uint64_t                           offset,
                                 struct iovec                      *iov,
                                 int                                iovcnt,
                                 ResponseHandler                   *handler,
                                 uint16_t                           timeout = 0 );

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
      static XRootDStatus Fcntl( std::shared_ptr<FileStateHandler> &self,
                                 const Buffer                      &arg,
                                 ResponseHandler                   *handler,
                                 uint16_t                           timeout = 0 );

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
      static XRootDStatus Visa( std::shared_ptr<FileStateHandler> &self,
                                ResponseHandler                   *handler,
                                uint16_t                           timeout = 0 );

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
      static XRootDStatus SetXAttr( std::shared_ptr<FileStateHandler> &self,
                                    const std::vector<xattr_t>        &attrs,
                                    ResponseHandler                   *handler,
                                    uint16_t                           timeout = 0 );

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
      static XRootDStatus GetXAttr( std::shared_ptr<FileStateHandler> &self,
                                    const std::vector<std::string>    &attrs,
                                    ResponseHandler                   *handler,
                                    uint16_t                           timeout = 0 );

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
      static XRootDStatus DelXAttr( std::shared_ptr<FileStateHandler> &self,
                                    const std::vector<std::string>    &attrs,
                                    ResponseHandler                   *handler,
                                    uint16_t                           timeout = 0 );

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
      static XRootDStatus ListXAttr( std::shared_ptr<FileStateHandler> &self,
                                     ResponseHandler                  *handler,
                                     uint16_t                          timeout = 0 );

      //------------------------------------------------------------------------
      //! Create a checkpoint
      //!
      //! @param handler : handler to be notified when the response arrives,
      //!                  the response parameter will hold a std::vector of
      //!                  XAttr objects
      //! @param timeout : timeout value, if 0 the environment default will
      //!                  be used
      //!
      //! @return        : status of the operation
      //------------------------------------------------------------------------
      static XRootDStatus Checkpoint( std::shared_ptr<FileStateHandler> &self,
                                      kXR_char                           code,
                                      ResponseHandler                   *handler,
                                      uint16_t                           timeout = 0 );

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
      static XRootDStatus ChkptWrt( std::shared_ptr<FileStateHandler> &self,
                                    uint64_t                           offset,
                                    uint32_t                           size,
                                    const void                        *buffer,
                                    ResponseHandler                   *handler,
                                    uint16_t                           timeout = 0 );

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
      static XRootDStatus ChkptWrtV( std::shared_ptr<FileStateHandler> &self,
                                     uint64_t                           offset,
                                     const struct iovec                *iov,
                                     int                                iovcnt,
                                     ResponseHandler                   *handler,
                                     uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Process the results of the opening operation
      //------------------------------------------------------------------------
      void OnOpen( const XRootDStatus *status,
                   const OpenInfo     *openInfo,
                   const HostList     *hostList );

      //------------------------------------------------------------------------
      //! Process the results of the closing operation
      //------------------------------------------------------------------------
      void OnClose( const XRootDStatus *status );

      //------------------------------------------------------------------------
      //! Handle an error while sending a stateful message
      //------------------------------------------------------------------------
      static void OnStateError( std::shared_ptr<FileStateHandler> &self,
                                XRootDStatus                      *status,
                                Message                           *message,
                                ResponseHandler                   *userHandler,
                                MessageSendParams                 &sendParams );

      //------------------------------------------------------------------------
      //! Handle stateful redirect
      //------------------------------------------------------------------------
      static void OnStateRedirection( std::shared_ptr<FileStateHandler> &self,
                                      const std::string                 &redirectUrl,
                                      Message                           *message,
                                      ResponseHandler                   *userHandler,
                                      MessageSendParams                 &sendParams );

      //------------------------------------------------------------------------
      //! Handle stateful response
      //------------------------------------------------------------------------
      static void OnStateResponse( std::shared_ptr<FileStateHandler> &self,
                                   XRootDStatus                      *status,
                                   Message                           *message,
                                   AnyObject                         *response,
                                   HostList                          *hostList );

      //------------------------------------------------------------------------
      //! Check if the file is open
      //------------------------------------------------------------------------
      bool IsOpen() const;

      //------------------------------------------------------------------------
      //! Check if the file is using an encrypted connection
      //------------------------------------------------------------------------
      inline bool IsSecure() const
      {
        return pIsChannelEncrypted;
      }

      //------------------------------------------------------------------------
      //! Set file property
      //!
      //! @see File::GetProperty for propert list
      //------------------------------------------------------------------------
      bool SetProperty( const std::string &name, const std::string &value );

      //------------------------------------------------------------------------
      //! Get file property
      //!
      //! @see File::SetProperty for property list
      //------------------------------------------------------------------------
      bool GetProperty( const std::string &name, std::string &value ) const;

      //------------------------------------------------------------------------
      //! Lock the internal lock
      //------------------------------------------------------------------------
      void Lock()
      {
        pMutex.Lock();
      }

      //------------------------------------------------------------------------
      //! Unlock the internal lock
      //------------------------------------------------------------------------
      void UnLock()
      {
        pMutex.UnLock();
      }

      //------------------------------------------------------------------------
      //! Tick
      //------------------------------------------------------------------------
      void Tick( time_t now );

      //------------------------------------------------------------------------
      //! Declare timeout on requests being recovered
      //------------------------------------------------------------------------
      void TimeOutRequests( time_t now );

      //------------------------------------------------------------------------
      //! Called in the child process after the fork
      //------------------------------------------------------------------------
      void AfterForkChild();

      //------------------------------------------------------------------------
      //! Try other data server
      //------------------------------------------------------------------------
      static XRootDStatus TryOtherServer( std::shared_ptr<FileStateHandler> &self,
                                          uint16_t                           timeout );

    private:
      //------------------------------------------------------------------------
      // Helper for queuing messages
      //------------------------------------------------------------------------
      struct RequestData
      {
        RequestData(): request(0), handler(0) {}
        RequestData( Message *r, ResponseHandler *h,
                     const MessageSendParams &p ):
          request(r), handler(h), params(p) {}
        Message           *request;
        ResponseHandler   *handler;
        MessageSendParams  params;
      };
      typedef std::list<RequestData> RequestList;

      //------------------------------------------------------------------------
      //! Generic implementation of xattr operation
      //!
      //! @param subcode : xattr operation code
      //! @param attrs   : operation argument
      //! @param handler : operation handler
      //! @param timeout : operation timeout
      //------------------------------------------------------------------------
      template<typename T>
      static Status XAttrOperationImpl( std::shared_ptr<FileStateHandler> &self,
                                        kXR_char                           subcode,
                                        kXR_char                           options,
                                        const std::vector<T>              &attrs,
                                        ResponseHandler                   *handler,
                                        uint16_t                           timeout = 0 );

      //------------------------------------------------------------------------
      //! Send a message to a host or put it in the recovery queue
      //------------------------------------------------------------------------
      static Status SendOrQueue( std::shared_ptr<FileStateHandler> &self,
                                 const URL                         &url,
                                 Message                           *msg,
                                 ResponseHandler                   *handler,
                                 MessageSendParams                 &sendParams );

      //------------------------------------------------------------------------
      //! Check if the stateful error is recoverable
      //------------------------------------------------------------------------
      bool IsRecoverable( const XRootDStatus &stataus ) const;

      //------------------------------------------------------------------------
      //! Recover a message
      //!
      //! @param rd                request data associated with the message
      //! @param callbackOnFailure should the current handler be called back
      //!                          if the recovery procedure fails
      //------------------------------------------------------------------------
      static Status RecoverMessage( std::shared_ptr<FileStateHandler> &self,
                                    RequestData                        rd,
                                    bool callbackOnFailure = true );

      //------------------------------------------------------------------------
      //! Run the recovery procedure if appropriate
      //------------------------------------------------------------------------
      static Status RunRecovery( std::shared_ptr<FileStateHandler> &self );

      //------------------------------------------------------------------------
      // Send a close and ignore the response
      //------------------------------------------------------------------------
      static XRootDStatus SendClose( std::shared_ptr<FileStateHandler> &self,
                                     uint16_t                           timeout );

      //------------------------------------------------------------------------
      //! Check if the file is open for read only
      //------------------------------------------------------------------------
      bool IsReadOnly() const;

      //------------------------------------------------------------------------
      //! Re-open the current file at a given server
      //------------------------------------------------------------------------
      static XRootDStatus ReOpenFileAtServer( std::shared_ptr<FileStateHandler> &self,
                                              const URL                         &url,
                                              uint16_t                           timeout );

      //------------------------------------------------------------------------
      //! Fail a message
      //------------------------------------------------------------------------
      void FailMessage( RequestData rd, XRootDStatus status );

      //------------------------------------------------------------------------
      //! Fail queued messages
      //------------------------------------------------------------------------
      void FailQueuedMessages( XRootDStatus status );

      //------------------------------------------------------------------------
      //! Re-send queued messages
      //------------------------------------------------------------------------
      void ReSendQueuedMessages();

      //------------------------------------------------------------------------
      //! Re-write file handle
      //------------------------------------------------------------------------
      void ReWriteFileHandle( Message *msg );

      //------------------------------------------------------------------------
      //! Reset monitoring vars
      //------------------------------------------------------------------------
      void ResetMonitoringVars()
      {
        pOpenTime.tv_sec = 0; pOpenTime.tv_usec = 0;
        pRBytes      = 0;
        pVRBytes     = 0;
        pWBytes      = 0;
        pVSegs       = 0;
        pRCount      = 0;
        pVRCount     = 0;
        pWCount      = 0;
        pCloseReason = Status();
      }

      //------------------------------------------------------------------------
      //! Dispatch monitoring information on close
      //------------------------------------------------------------------------
      void MonitorClose( const XRootDStatus *status );

      //------------------------------------------------------------------------
      //! Issues request:
      //!  - if the request is for a Metalink a redirect is generated
      //!  - if the request is for a local file, it will be passed to
      //!    LocalFileHandler
      //!  - otherwise vanilla XRootD request will be sent
      //------------------------------------------------------------------------
      XRootDStatus IssueRequest( const URL         &url,
                                 Message           *msg,
                                 ResponseHandler   *handler,
                                 MessageSendParams &sendParams );

      //------------------------------------------------------------------------
      //! Send a write request with payload being stored in a kernel buffer
      //------------------------------------------------------------------------
      static XRootDStatus WriteKernelBuffer( std::shared_ptr<FileStateHandler>     &self,
                                             uint64_t                               offset,
                                             uint32_t                               length,
                                             std::unique_ptr<XrdSys::KernelBuffer>  kbuff,
                                             ResponseHandler                       *handler,
                                             uint16_t                               timeout );

      mutable XrdSysMutex     pMutex;
      FileStatus              pFileState;
      XRootDStatus            pStatus;
      StatInfo               *pStatInfo;
      URL                    *pFileUrl;
      URL                    *pDataServer;
      URL                    *pLoadBalancer;
      URL                    *pStateRedirect;
      URL                    *pWrtRecoveryRedir;
      uint8_t                *pFileHandle;
      uint16_t                pOpenMode;
      uint16_t                pOpenFlags;
      RequestList             pToBeRecovered;
      std::set<Message*>      pInTheFly;
      uint64_t                pSessionId;
      bool                    pDoRecoverRead;
      bool                    pDoRecoverWrite;
      bool                    pFollowRedirects;
      bool                    pUseVirtRedirector;
      bool                    pIsChannelEncrypted;
      bool                    pAllowBundledClose;

      //------------------------------------------------------------------------
      // Monitoring variables
      //------------------------------------------------------------------------
      timeval                  pOpenTime;
      uint64_t                 pRBytes;
      uint64_t                 pVRBytes;
      uint64_t                 pWBytes;
      uint64_t                 pVWBytes;
      uint64_t                 pVSegs;
      uint64_t                 pRCount;
      uint64_t                 pVRCount;
      uint64_t                 pWCount;
      uint64_t                 pVWCount;
      XRootDStatus             pCloseReason;

      //------------------------------------------------------------------------
      // Responsible for file:// operations on the local filesystem
      //------------------------------------------------------------------------
      LocalFileHandler      *pLFileHandler;

      //------------------------------------------------------------------------
      // Responsible for Writing/Reading erasure-coded files
      //------------------------------------------------------------------------
      FilePlugIn           *&pPlugin;
  };
}

#endif // __XRD_CL_FILE_STATE_HANDLER_HH__

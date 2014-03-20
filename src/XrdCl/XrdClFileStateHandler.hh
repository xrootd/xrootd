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
#include "XrdSys/XrdSysPthread.hh"
#include <list>
#include <set>

namespace XrdCl
{
  class Message;

  //----------------------------------------------------------------------------
  //! Handle the stateful operations
  //----------------------------------------------------------------------------
  class FileStateHandler
  {
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
      FileStateHandler();

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
      XRootDStatus Open( const std::string &url,
                         uint16_t           flags,
                         uint16_t           mode,
                         ResponseHandler   *handler,
                         uint16_t           timeout  = 0 );

      //------------------------------------------------------------------------
      //! Close the file object
      //!
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( ResponseHandler *handler,
                          uint16_t         timeout = 0 );

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
      XRootDStatus Read( uint64_t         offset,
                         uint32_t         size,
                         void            *buffer,
                         ResponseHandler *handler,
                         uint16_t         timeout = 0 );

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
      XRootDStatus Write( uint64_t         offset,
                          uint32_t         size,
                          const void      *buffer,
                          ResponseHandler *handler,
                          uint16_t         timeout = 0 );


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
      //! Read scattered data chunks in one operation - async
      //!
      //! @param chunks    list of the chunks to be read
      //! @param buffer    a pointer to a buffer big enough to hold the data
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
      void OnStateError( XRootDStatus        *status,
                         Message             *message,
                         ResponseHandler     *userHandler,
                         MessageSendParams   &sendParams );

      //------------------------------------------------------------------------
      //! Handle stateful redirect
      //------------------------------------------------------------------------
      void OnStateRedirection( const std::string &redirectUrl,
                               Message           *message,
                               ResponseHandler   *userHandler,
                               MessageSendParams &sendParams );

      //------------------------------------------------------------------------
      //! Handle stateful response
      //------------------------------------------------------------------------
      void OnStateResponse( XRootDStatus *status,
                            Message      *message,
                            AnyObject    *response,
                            HostList     *hostList );

      //------------------------------------------------------------------------
      //! Check if the file is open
      //------------------------------------------------------------------------
      bool IsOpen() const;

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
      //! Send a message to a host or put it in the recovery queue
      //------------------------------------------------------------------------
      Status SendOrQueue( const URL         &url,
                          Message           *msg,
                          ResponseHandler   *handler,
                          MessageSendParams &sendParams );

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
      Status RecoverMessage( RequestData rd, bool callbackOnFailure = true );

      //------------------------------------------------------------------------
      //! Run the recovery procedure if appropriate
      //------------------------------------------------------------------------
      Status RunRecovery();

      //------------------------------------------------------------------------
      // Send a close and ignore the response
      //------------------------------------------------------------------------
      Status SendClose( uint16_t timeout );

      //------------------------------------------------------------------------
      //! Check if the file is open for read only
      //------------------------------------------------------------------------
      bool IsReadOnly() const;

      //------------------------------------------------------------------------
      //! Re-open the current file at a given server
      //------------------------------------------------------------------------
      Status ReOpenFileAtServer( const URL &url, uint16_t timeout );

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
        pVBytes      = 0;
        pWBytes      = 0;
        pVSegs       = 0;
        pRCount      = 0;
        pVCount      = 0;
        pWCount      = 0;
        pCloseReason = Status();
      }

      //------------------------------------------------------------------------
      //! Dispatch monitoring information on close
      //------------------------------------------------------------------------
      void MonitorClose( const XRootDStatus *status );

      mutable XrdSysMutex     pMutex;
      FileStatus              pFileState;
      XRootDStatus            pStatus;
      StatInfo               *pStatInfo;
      URL                    *pFileUrl;
      URL                    *pDataServer;
      URL                    *pLoadBalancer;
      URL                    *pStateRedirect;
      uint8_t                *pFileHandle;
      uint16_t                pOpenMode;
      uint16_t                pOpenFlags;
      RequestList             pToBeRecovered;
      std::set<Message*>      pInTheFly;
      uint64_t                pSessionId;
      bool                    pDoRecoverRead;
      bool                    pDoRecoverWrite;
      bool                    pFollowRedirects;

      //------------------------------------------------------------------------
      // Monitoring variables
      //------------------------------------------------------------------------
      timeval                  pOpenTime;
      uint64_t                 pRBytes;
      uint64_t                 pVBytes;
      uint64_t                 pWBytes;
      uint64_t                 pVSegs;
      uint64_t                 pRCount;
      uint64_t                 pVCount;
      uint64_t                 pWCount;
      XRootDStatus             pCloseReason;
  };
}

#endif // __XRD_CL_FILE_STATE_HANDLER_HH__

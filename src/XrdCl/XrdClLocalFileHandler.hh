//------------------------------------------------------------------------------
// Copyright (c) 2017 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH 
// Author: Paul-Niklas Kramp <p.n.kramp@gsi.de>
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
#ifndef __XRD_CL_LOCAL_FILE_HANDLER_HH__
#define __XRD_CL_LOCAL_FILE_HANDLER_HH__
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClLocalFileTask.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"

#include <sys/uio.h>

namespace XrdCl
{
  class Message;
  struct MessageSendParams;

  class LocalFileHandler
  {
    public:

      LocalFileHandler();

      ~LocalFileHandler();

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
      XRootDStatus Open( const std::string &url, uint16_t flags, uint16_t mode,
          ResponseHandler *handler, time_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Handle local redirect to given URL triggered by the given request
      //------------------------------------------------------------------------
      XRootDStatus Open( const URL *url, const Message *req, AnyObject *&resp );

      //------------------------------------------------------------------------
      //! Close the file object
      //!
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( ResponseHandler *handler, time_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain status information for this file - async
      //!
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a StatInfo object
      //!                if the procedure is successful
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Stat( ResponseHandler *handler, time_t timeout = 0 );

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
      XRootDStatus Read( uint64_t offset, uint32_t size, void *buffer,
          ResponseHandler *handler, time_t timeout = 0 );

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
                          time_t           timeout = 0 );

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
      XRootDStatus Write( uint64_t offset, uint32_t size, const void *buffer,
          ResponseHandler *handler, time_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Commit all pending disk writes - async
      //!
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Sync( ResponseHandler *handler, time_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Truncate the file to a particular size - async
      //!
      //! @param size    desired size of the file
      //! @param handler handler to be notified when the response arrives
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Truncate( uint64_t size, ResponseHandler *handler,
          time_t timeout = 0 );

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
      XRootDStatus VectorRead( const ChunkList &chunks, void *buffer,
          ResponseHandler *handler, time_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Write scattered data chunks in one operation - async
      //!
      //! @param chunks    list of the chunks to be read
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus VectorWrite( const ChunkList &chunks,
          ResponseHandler *handler, time_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Write scattered buffers in one operation - async
      //!
      //! @param offset    offset from the beginning of the file
      //! @param chunks    list of the chunks to be read
      //! @param handler   handler to be notified when the response arrives
      //! @param timeout   timeout value, if 0 then the environment default
      //!                  will be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus WriteV( uint64_t            offset,
                           ChunkList          *chunks,
                           ResponseHandler    *handler,
                           time_t              timeout = 0 );

      //------------------------------------------------------------------------
      //! Queues a task to the jobmanager
      //!
      //! @param st        the status of the file operation
      //! @param obj       the object holding data like open-, chunk- or vreadinfo
      //! @param handler   handler to be notified when the response arrives
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus QueueTask( XRootDStatus *st, AnyObject *obj,
          ResponseHandler *handler );

      //------------------------------------------------------------------------
      //! Performs a custom operation on an open file - async
      //!
      //! @param arg       query argument
      //! @param handler   handler to be notified when the response arrives,
      //!                  the response parameter will hold a Buffer object
      //!                  if the procedure is successful
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Fcntl( const Buffer &arg, ResponseHandler *handler,
          time_t timeout = 0 );

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
      XRootDStatus Visa( ResponseHandler *handler, time_t timeout = 0 );


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
      XRootDStatus SetXAttr( const std::vector<xattr_t> &attrs,
                             ResponseHandler            *handler,
                             time_t                      timeout = 0 );

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
      XRootDStatus GetXAttr( const std::vector<std::string> &attrs,
                             ResponseHandler                *handler,
                             time_t                          timeout = 0 );

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
      XRootDStatus DelXAttr( const std::vector<std::string> &attrs,
                             ResponseHandler                *handler,
                             time_t                          timeout = 0 );

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
                              time_t                     timeout = 0 );

      //------------------------------------------------------------------------
      //! creates the directories specified in path
      //!
      //! @param path      specifies which directories are to be created
      //! @return          status of the mkdir system call
      //------------------------------------------------------------------------
      static XRootDStatus MkdirPath( const std::string &path );

      void SetHostList( const HostList &hostList )
      {
        pHostList = hostList;
      }

      const HostList& GetHostList()
      {
        return pHostList;
      }

      //------------------------------------------------------------------------
      //! Translate an XRootD request into LocalFileHandler call
      //------------------------------------------------------------------------
      XRootDStatus ExecRequest( const URL         &url,
                                Message           *msg,
                                ResponseHandler   *handler,
                                MessageSendParams &sendParams );

    private:

      XRootDStatus OpenImpl( const std::string &url, uint16_t flags,
                             uint16_t mode, AnyObject *&resp );

      //------------------------------------------------------------------------
      //! Parses kXR_fattr request and calls respective XAttr operation
      //------------------------------------------------------------------------
      XRootDStatus XAttrImpl( kXR_char          code,
                              kXR_char          numattr,
                              size_t         bodylen,
                              char             *body,
                              ResponseHandler  *handler );

      //---------------------------------------------------------------------
      // Internal filedescriptor, which is used by all operations after open
      //---------------------------------------------------------------------
      int fd;

      //---------------------------------------------------------------------
      // The file URL
      //---------------------------------------------------------------------
      std::string pUrl;

      //---------------------------------------------------------------------
      // The host list returned in the user callback
      //---------------------------------------------------------------------
      HostList pHostList;

  };
}
#endif

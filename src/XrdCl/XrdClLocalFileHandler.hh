#ifndef __XRD_CL_LOCAL_FILE_HANDLER_HH__
#define __XRD_CL_LOCAL_FILE_HANDLER_HH__
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClLocalFileTask.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"

namespace XrdCl
{
    class LocalFileHandler{
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
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a StatInfo object
      //!                if the procedure is successful
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Stat( ResponseHandler *handler,
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

        private:
            JobManager *jmngr;
            int fd;

    };
}
#endif
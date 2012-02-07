//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_FILE_HH__
#define __XRD_CL_FILE_HH__

#include "XrdCl/XrdClQuery.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include <stdint.h>
#include <string>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! A file
  //----------------------------------------------------------------------------
  class File
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      File() {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~File() { Close(); }

      //------------------------------------------------------------------------
      //! Open the file pointed to by the given URL - async
      //!
      //! @param url     url of the file to be opened
      //! @param flags   OpenFlags::Flags
      //! @param mode    Access::Mode
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
      //! Open the file pointed to by the given URL - sync
      //!
      //! @param url     url of the file to be opened
      //! @param flags   OpenFlags::Flags
      //! @param mode    Access::Mode
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Open( const std::string &url,
                         uint16_t           flags,
                         uint16_t           mode,
                         uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Close the file
      //------------------------------------------------------------------------
      XRootDStatus Close();

      //------------------------------------------------------------------------
      //! Read a data chunk at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be read
      //! @param buffer  a pointer to a buffer big enough to hold the data
      //!                or 0 if the buffer should be allocated by the system
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a buffer object if
      //!                the procedure was successful, if a prealocated
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
      //! Read a data chunk at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be read
      //! @param buffer  a pointer to a buffer big enough to hold the data
      //!                or 0 if the buffer should be allocated by the system
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Read( uint64_t  offset,
                         uint32_t  size,
                         void     *buffer,
                         uint16_t  timeout = 0 );

      //------------------------------------------------------------------------
      //! Write a data chank at a given offset - async
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
                          void            *buffer,
                          ResponseHandler *handler,
                          uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Write a data chunk at a given offset - sync
      //!
      //! @param offset  offset from the beginning of the file
      //! @param size    number of bytes to be written
      //! @param buffer  a pointer to the buffer holding the data to be written
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t  offset,
                          uint32_t  size,
                          void     *buffer,
                          uint16_t  timeout = 0 );

    private:
  };
}

#endif // __XRD_CL_FILE_HH__

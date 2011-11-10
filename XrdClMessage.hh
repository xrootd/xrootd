//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_MESSAGE_HH__
#define __XRD_CL_MESSAGE_HH__

#include <cstdlib>
#include <stdint.h>
#include <new>

namespace XrdClient
{
  class Message
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Message( uint32_t size = 0 ): pBuffer(0), pSize(0)
      {
        if( size )
        {
          Allocate( size );
          Zero();
        }
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Message() { Free(); }

      //------------------------------------------------------------------------
      //! Get the message buffer
      //------------------------------------------------------------------------
      const char *GetBuffer( uint32_t offset = 0 ) const
      {
        return pBuffer+offset;
      }

      //------------------------------------------------------------------------
      //! Get the message buffer
      //------------------------------------------------------------------------
      char *GetBuffer( uint32_t offset = 0 )
      {
        return pBuffer+offset;
      }

      //------------------------------------------------------------------------
      //! Reallocate the buffer to a new location of a given size
      //------------------------------------------------------------------------
      void ReAllocate( uint32_t size )
      {
        pBuffer = (char *)realloc( pBuffer, size );
        if( !pBuffer )
          throw std::bad_alloc();
        pSize = size;
      }

      //------------------------------------------------------------------------
      //! Free the buffer
      //------------------------------------------------------------------------
      void Free()
      {
        free( pBuffer );
        pBuffer = 0;
        pSize   = 0;
      }

      //------------------------------------------------------------------------
      //! Allocate the buffer
      //------------------------------------------------------------------------
      void Allocate( uint32_t size )
      {
        pBuffer = (char *)malloc( size );
        if( !pBuffer )
          throw std::bad_alloc();
        pSize = size;
      }

      //------------------------------------------------------------------------
      //! Zero
      //------------------------------------------------------------------------
      void Zero()
      {
        memset( pBuffer, 0, pSize );
      }

      //------------------------------------------------------------------------
      //! Get the size of the message
      //------------------------------------------------------------------------
      uint32_t GetSize() const
      {
        return pSize;
      }

    private:
      char     *pBuffer;
      uint32_t  pSize;
  };
}

#endif // __XRD_CL_MESSAGE_HH__

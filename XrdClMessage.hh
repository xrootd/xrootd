//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
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
      Message( uint32_t size = 0 ): pBuffer(0), pSize(0), pCursor(0)
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

      //------------------------------------------------------------------------fss
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
        pCursor = 0;
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

      //------------------------------------------------------------------------
      //! Get append cursor
      //------------------------------------------------------------------------
      uint32_t GetCursor() const
      {
        return pCursor;
      }

      //------------------------------------------------------------------------
      //! Set the cursor
      //------------------------------------------------------------------------
      void SetCursor( uint32_t cursor )
      {
        pCursor = cursor;
      }

      //------------------------------------------------------------------------
      //! Advance the cursor
      //------------------------------------------------------------------------
      void AdvanceCursor( uint32_t delta )
      {
        pCursor += delta;
      }

      //------------------------------------------------------------------------
      //! Append data at the position pointed to by the append cursor
      //------------------------------------------------------------------------
      bool Append( char *buffer, uint32_t size )
      {
        if( pCursor >= pSize )
          return false;

        uint32_t remaining = pSize-pCursor;
        if( remaining < size )
          ReAllocate( pCursor+size );

        memcpy( pBuffer+pCursor, buffer, size );
        pCursor += size;
      }

      //------------------------------------------------------------------------
      //! Get the buffer pointer at the append cursor
      //------------------------------------------------------------------------
      char *GetBufferAtCursor()
      {
        return GetBuffer( pCursor );
      }

      const char *GetBufferAtCursor() const
      {
        return GetBuffer( pCursor );
      }

    private:
      char     *pBuffer;
      uint32_t  pSize;
      uint32_t  pCursor;
  };
}

#endif // __XRD_CL_MESSAGE_HH__

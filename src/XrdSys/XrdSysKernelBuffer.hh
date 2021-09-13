//-----------------------------------------------------------------------------
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------

#ifndef SRC_XRDCL_XRDCLKERNELBUFFER_HH_
#define SRC_XRDCL_XRDCLKERNELBUFFER_HH_

#include <fcntl.h>
#include <sys/uio.h>
#include <cstdint>
#include <unistd.h>
#include <cstring>

#include <vector>
#include <array>
#include <tuple>

namespace XrdSys
{
  //---------------------------------------------------------------------------
  //! A utility class for manipulating kernel space buffers
  //!
  //! (Encapsulates the splice vmsplice & syscalls.)
  //---------------------------------------------------------------------------
  class KernelBuffer
  {
      friend ssize_t Read( int, KernelBuffer&, uint32_t, int64_t );

      friend ssize_t Read( int, KernelBuffer&, uint32_t );

      friend ssize_t Write( int, KernelBuffer&, int64_t );

      friend ssize_t Send( int, KernelBuffer& );

      friend ssize_t Move( KernelBuffer&, char*& );

      friend ssize_t Move( char*&, KernelBuffer&, size_t );

    public:

      //-----------------------------------------------------------------------
      //! Default constructor
      //-----------------------------------------------------------------------
      KernelBuffer() : capacity( 0 ), size( 0 )
      {
      }

      //-----------------------------------------------------------------------
      //! Copy constructor - deleted
      //-----------------------------------------------------------------------
      KernelBuffer( const KernelBuffer& ) = delete;

      //-----------------------------------------------------------------------
      // Move constructor
      //-----------------------------------------------------------------------
      KernelBuffer( KernelBuffer &&kbuff ) : capacity( kbuff.capacity ),
                                             size( kbuff.size ),
                                             pipes( std::move( kbuff.pipes ) )
      {
        capacity = 0;
        size     = 0;
      }

      //-----------------------------------------------------------------------
      //! Copy assignment operator - deleted
      //-----------------------------------------------------------------------
      KernelBuffer& operator=( const KernelBuffer& ) = delete;

      //-----------------------------------------------------------------------
      //! Move assignment operator
      //-----------------------------------------------------------------------
      KernelBuffer& operator=( KernelBuffer &&kbuff )
      {
        capacity = kbuff.capacity;
        size     = kbuff.size;
        pipes    = std::move( kbuff.pipes );
        return *this;
      }

      //-----------------------------------------------------------------------
      //! Destructor
      //-----------------------------------------------------------------------
      ~KernelBuffer()
      {
        if( capacity > 0 ) Free();
      }

      //------------------------------------------------------------------------
      //! @return : true is buffer is empty, false otherwise
      //------------------------------------------------------------------------
      inline bool Empty() const
      {
        return size == 0;
      }

      //-----------------------------------------------------------------------
      //! Check if the user space buffer is page aligned
      //!
      //! @param ptr : user space buffer
      //!
      //! @return    : true if the buffer is page aligned, false otherwise
      //-----------------------------------------------------------------------
      inline static bool IsPageAligned( const void *ptr )
      {
        return ( ( uintptr_t ( ptr ) ) % PAGE_SZ ) == 0 ;
      }

    private:

      //-----------------------------------------------------------------------
      //! Closes the underlying pipes (kernel buffers)
      //-----------------------------------------------------------------------
      inline void Free()
      {
        auto itr = pipes.begin();
        for( ; itr != pipes.end() ; ++itr )
        {
          std::array<int, 2> &p = std::get<0>( *itr );
          close( p[1] );
          close( p[0] );
        }

        pipes.clear();
        capacity = 0;
        size = 0;
      }

      //-----------------------------------------------------------------------
      //! Allocates another pipe (kernel buffer) of size up to 1MB
      //-----------------------------------------------------------------------
      inline ssize_t Alloc( size_t size )
      {
#ifndef F_SETPIPE_SZ
        return -ENOTSUP;
#else
        ssize_t ret = 0;

        std::array<int, 2> pipe_fd;
        ret = pipe( pipe_fd.data() );
        if( ret < 0 ) return ret;

        if( size > MAX_PIPE_SIZE) size = MAX_PIPE_SIZE;
        ret = fcntl( pipe_fd[0], F_SETPIPE_SZ, size );
        if( ret < 0 ) return ret;

        capacity += ret;
        pipes.emplace_back( pipe_fd, 0 );

        return ret;
#endif
      }

      //-----------------------------------------------------------------------
      //! Read data from a file descriptor to a kernel buffer
      //!
      //! @param fd     : file descriptor
      //! @param length : amount of data to be read
      //! @param offset : offset of the data in the source file
      //!
      //! @return       : size of the data read from the file descriptor or -1
      //!                 on error
      //-----------------------------------------------------------------------
#ifndef SPLICE_F_MOVE
      inline ssize_t ReadFromFD( int fd, uint32_t length, int64_t *offset )
      {
        return -ENOTSUP;
      }
#else
      inline ssize_t ReadFromFD( int fd, uint32_t length, loff_t *offset )
      {
        if( capacity > 0 ) Free();

        while( length > 0 )
        {
          ssize_t ret = Alloc( length );
          if( ret < 0 ) return ret;
          if( size_t( ret ) > length ) ret = length;
          std::array<int, 2> &pipe_fd  = std::get<0>( pipes.back() );
          size_t             &pipedata = std::get<1>( pipes.back() );
          ret = splice( fd, offset, pipe_fd[1], NULL, ret, SPLICE_F_MOVE | SPLICE_F_MORE );
          if( ret == 0 ) break; // we reached the end of the file
          if( ret < 0 ) return -1;
          pipedata += ret;

          length -= ret;
          size += ret;
        }
        pipes_cursor = pipes.begin();

        return size;
      }
#endif

      //-----------------------------------------------------------------------
      //! Write data from a kernel buffer to a file descriptor
      //!
      //! @param fd     : file descriptor
      //! @param offset : offset in the target file
      //!
      //! @return       : size of the data written into the file descriptor or
      //!                 -1 on error
      //-----------------------------------------------------------------------
#ifndef SPLICE_F_MOVE
      inline ssize_t WriteToFD( int fd, int64_t *offset )
      {
        return -ENOTSUP;
      }
#else
      inline ssize_t WriteToFD( int fd, loff_t *offset )
      {
        if( size == 0 ) return 0;

        ssize_t result = 0;

        auto itr = pipes_cursor;
        while( itr != pipes.end() )
        {
          std::array<int, 2> &pipe_fd  = std::get<0>( *itr );
          size_t             &pipedata = std::get<1>( *itr );

          int ret = splice( pipe_fd[0], NULL, fd, offset, size, SPLICE_F_MOVE | SPLICE_F_MORE );
          if( ret == 0 ) break; // we reached the end of the file
          if( ret < 0 ) return -1;

          size     -= ret;
          result   += ret;
          pipedata -= ret;


          if( pipedata > 0 ) continue;

          ++pipes_cursor;
          ++itr;
        }

        Free();

        return result;
      }
#endif

      //-----------------------------------------------------------------------
      //! Move the buffer to user space:
      //!
      //! @param buffer : a user space buffer containing the data, allocated by
      //!                 ToUser routine, to be deallocated with free()
      //!
      //! @return       : number of bytes transferred to user space or -1 on
      //!                 error
      //!
      //! Note:
      //! vmsplice() really supports true splicing only from user memory to a
      //! pipe.  In the opposite direction, it actually just copies the data to
      //! userspace.  But this makes the interface nice and symmetric and
      //! enables people to build on vmsplice() with room for future
      //! improvement in performance.
      //-----------------------------------------------------------------------
      inline ssize_t ToUser( char *&buffer )
      {
#ifndef SPLICE_F_MOVE
        return -ENOTSUP;
#else
        if( size == 0 ) return 0;

        ssize_t result = 0;

        void *void_ptr = 0;
        int ret = posix_memalign( &void_ptr, PAGE_SZ, size );
        if( ret )
        {
          errno = ret;
          return -1;
        }
        char *ptr = reinterpret_cast<char*>( void_ptr );

        auto itr = pipes_cursor;
        while( itr != pipes.end() )
        {
          iovec iov[1];
          size_t len = size > MAX_PIPE_SIZE ? MAX_PIPE_SIZE : size;
          iov->iov_len  = len;
          iov->iov_base = ptr;

          std::array<int, 2> &pipe_fd  = std::get<0>( *itr );
          size_t             &pipedata = std::get<1>( *itr );
          int ret = vmsplice( pipe_fd[0], iov, 1, 0 );  // vmsplice man NOTE:
                                                        // vmsplice() really supports true splicing only from user memory to a
                                                        // pipe.  In the opposite direction, it actually just copies the data to
                                                        // userspace.  But this makes the interface nice and symmetric and
                                                        // enables people to build on vmsplice() with room for future
                                                        // improvement in performance.
          if( ret < 0 ) // an error
          {
            delete[] buffer;
            buffer = 0;
            return ret;
          }

          size     -= ret;
          ptr      += ret;
          result   += ret;
          pipedata -= ret;

          if( pipedata > 0 ) continue;

          ++itr;
          ++pipes_cursor;
        }

        Free();

        buffer = reinterpret_cast<char*>( void_ptr );
        return result;
#endif
      }

      //-----------------------------------------------------------------------
      //! Move a buffer from user space to kernel space
      //!
      //! @param buffer : buffer to be moved to kernel space (needs to be page
      //!                 aligned and deallocable with free)
      //! @param length : length of the buffer
      //!
      //! @return       : number of bytes transferred to kernel space or -1 on
      //!                 error
      //!
      //! NOTE: on success the user space buffer will be freed and the buffer
      //!       will be set to NULL
      //!
      //! NOTE:
      //! The user pages are a gift to the kernel.  The application may
      //! not modify this memory ever, otherwise the page cache and on-
      //! disk data may differ.
      //-----------------------------------------------------------------------
      inline ssize_t FromUser( char *&buffer, size_t length )
      {
#ifndef SPLICE_F_MOVE
        return -ENOTSUP;
#else
        if( !IsPageAligned( buffer ) )
        {
          errno = EINVAL;
          return -1;
        }

        if( capacity > 0 ) Free();

        char *buff = buffer;
        while( length > 0 )
        {
          ssize_t ret = Alloc( length );
          if( ret < 0 ) return ret;
          std::array<int, 2> &pipe_fd  = std::get<0>( pipes.back() );
          size_t             &pipedata = std::get<1>( pipes.back() );

          iovec iov[1];
          iov->iov_len  = size_t( ret ) < length ? ret : length;
          iov->iov_base = buff;
          ret = vmsplice( pipe_fd[1], iov, 1, SPLICE_F_GIFT );

          if( ret < 0 ) return -1;
          length   -= ret;
          size     += ret;
          buff     += ret;
          pipedata += ret;
        }

        pipes_cursor = pipes.begin();
        free( buffer );
        buffer = 0;
        return size;
#endif
      }

      static const size_t PAGE_SZ       =    4 * 1024; //< page size
      static const size_t MAX_PIPE_SIZE = 1024 * 1024; //< maximum pipe size

      size_t capacity; //< the total capacity of all underlying pipes
      size_t size; //< size of the data stored in this kernel buffer
      std::vector<std::tuple<std::array<int,2>, size_t>> pipes; //< the unerlying pipes
      std::vector<std::tuple<std::array<int,2>, size_t>>::iterator pipes_cursor;
  };

  //---------------------------------------------------------------------------
  //! Utility function for reading data from a file descriptor into a kernel
  //! buffer.
  //!
  //! @see KernelBuffer::ReadFromFD
  //---------------------------------------------------------------------------
  inline ssize_t Read( int fd, KernelBuffer &buffer, uint32_t length, int64_t offset )
  {
    return buffer.ReadFromFD( fd, length, &offset );
  }

  //---------------------------------------------------------------------------
  //! Utility function for reading data from a file descriptor into a kernel
  //! buffer.
  //!
  //! @see KernelBuffer::ReadFromFD
  //---------------------------------------------------------------------------
  inline ssize_t Read( int fd, KernelBuffer &buffer, uint32_t length )
  {
    return buffer.ReadFromFD( fd, length, NULL );
  }

  //---------------------------------------------------------------------------
  //! Utility function for writing data from a kernel buffer into a file
  //! descriptor.
  //!
  //! @see KernelBuffer::WriteToFD
  //---------------------------------------------------------------------------
  inline ssize_t Write( int fd, KernelBuffer &buffer, int64_t offset )
  {
    return buffer.WriteToFD( fd, &offset );
  }

  //---------------------------------------------------------------------------
  //! Utility function for sending data from a kernel buffer into a socket.
  //!
  //! @see KernelBuffer::WriteToFD
  //---------------------------------------------------------------------------
  inline ssize_t Send( int fd, KernelBuffer &buffer )
  {
    return buffer.WriteToFD( fd, NULL );
  }

  //---------------------------------------------------------------------------
  //! Utility function for moving a kernel buffer to user space.
  //!
  //! @see KernelBuffer::ToUser
  //---------------------------------------------------------------------------
  inline ssize_t Move( KernelBuffer &kbuff, char *&ubuff )
  {
    return kbuff.ToUser( ubuff );
  }

  //---------------------------------------------------------------------------
  //! Utility function for moving a user space buffer to kernel space.
  //!
  //! @see KernelBuffer::FromUser
  //---------------------------------------------------------------------------
  inline ssize_t Move( char *&ubuff, KernelBuffer &kbuff, size_t length )
  {
    return kbuff.FromUser( ubuff, length );
  }

}


#endif /* SRC_XRDCL_XRDCLKERNELBUFFER_HH_ */

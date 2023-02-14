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

#ifndef SRC_XRDSYS_XRDSYSSHAREDMEMORY_HH_
#define SRC_XRDSYS_XRDSYSSHAREDMEMORY_HH_

#include <string>
#include <tuple>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace XrdSys
{
  /**
   * A utility class for handling shared memory
   */
  class SharedMemory
  {
    public:

      /**
       * Constructor
       * @param name : the name of the shared memory segment
       *               (shared memory object should be identified
       *               by a name of the form /somename)
       */
      SharedMemory( const std::string &name ):
        name( name ), size( 0 ), fd( -1 ), ptr( nullptr ){ }

      /**
       * Destructor (unmaps the shared memory segment)
       */
      ~SharedMemory()
      {
        if( ptr )
          munmap( ptr, size ); /* log errors */
      }

      /**
       * Create a new shared memory segment
       * @param size : size of the memory segment
       * @return     : 0 on success, -1 otherwise (check errno)
       */
      int Create( size_t size )
      {
        this->size = size;
        fd = shm_open( name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600 );
        if( fd < 0 )
          return -1;
        if( ftruncate( fd, size ) < 0 )
          return -1;
        Map();
        if( ptr == nullptr )
          return -1;
        return 0;
      }

      /**
       * Open a shared memory segment
       *
       * @return : 0 on success, -1 otherwise (check errno)
       */
      int Open()
      {
        fd = shm_open( name.c_str(), O_RDWR, 0600 );
        if( fd < 0 )
          return -1;
        struct stat statbuf;
        if( fstat( fd, &statbuf ) < 0 )
          return -1;
        size = statbuf.st_size;
        Map();
        if( ptr == nullptr )
          return -1;
        return 0;
      }

      /**
       * @return : pointer to the shared memory segment and its size
       */
      inline std::tuple<void*, size_t> Get()
      {
        return std::make_tuple( ptr, size );
      }

      /**
       * Yields the ownership of the shared memory segment
       *
       * @return : pointer to the shared memory segment and its size
       */
      inline std::tuple<void*, size_t> Move()
      {
        auto tpl = std::make_tuple( ptr, size );
        ptr  = nullptr;
        size = 0;
        return tpl;
      }

      /**
       * Close the file descriptor corresponding to the
       * shared memory segment (can be safely done after
       * a pointer to the shared memory has been obtained)
       *
       * @return : 0 on success, -1 otherwise (check errno)
       */
      inline int Close()
      {
        int rc = close( fd );
        if( rc == 0 )
          fd = -1;
        return rc;
      }

      /**
       * Destroy the shared memory segment:
       * - unmap the memory if necessary
       * - close the file descriptor if necessary
       * - unlink the shared memory block
       *
       * @return : 0 on success, -1 otherwise (check errno)
       */
      int Destroy()
      {
        if( ptr )
        {
          if( munmap( ptr, size ) < 0 )
            return -1;
          ptr = nullptr;
        }
        if( fd != -1 )
        {
          if( close( fd ) < 0 )
            return -1;
        }
        return shm_unlink( name.c_str() );
      }

    private:

      /**
       * Map the shared memory block into processe's virtual
       * address space.
       */
      inline void Map()
      {
        ptr = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if( ptr == MAP_FAILED )
          ptr = nullptr;
      }

      std::string name; //> name of shared memory segment
      size_t size; //> size of the shared memory segment
      int fd; //> file descriptor pointing to the shared memory
      void *ptr; //> pointer to the shared memory
  };
}


#endif /* SRC_XRDSYS_XRDSYSSHAREDMEMORY_HH_ */

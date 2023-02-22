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

#ifndef SRC_XRDSYS_XRDSYSSHMEM_HH_
#define SRC_XRDSYS_XRDSYSSHMEM_HH_

#include <string>
#include <tuple>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace XrdSys
{

  /**
   * Shared memory exception type.
   */
  struct shm_error : public std::exception
  {
    shm_error( int errcode ) :
      errcode( errcode ), errmsg( strerror( errcode )){ }

    const int errcode; //> errno
    const std::string errmsg; //> error message
  };

  /**
   * Utility class for creating and obtaining shared emory
   */
  struct shm
  {
    /**
     * Helper function for creating shared memory block
     *
     * @param name : name of the shared memory block (shared
     *               memory object should be identified by a
     *               name of the form /somename)
     * @param size : size of the shared memory segment
     * @return     : pointer to the shared memory and its size
     */
    inline static std::tuple<void*, size_t> create( const std::string &name, size_t size )
    {
	  int fd = shm_open( name.c_str(), O_CREAT | O_RDWR, 0600 );
      if( fd < 0 )
	    throw shm_error( errno );
	  if( ftruncate( fd, size ) < 0 )
	  {
        if( errno != EINVAL )
          throw shm_error( errno );
	  }
	  struct stat statbuf;
	  if( fstat( fd, &statbuf ) < 0 )
	    throw shm_error( errno );
	  size = statbuf.st_size;
	  void *mem = map_shm( fd, size );
	  close( fd );
	  return std::make_tuple( mem, size );
    }

    /**
     * Helper function for getting shared memory block
     *
     * @param name : name of the shared memory block (shared
     *               memory object should be identified by a
     *               name of the form /somename)
     * @return     : pointer to the shared memory and its size
     */
    template<typename T>
    inline static std::tuple<T*, size_t> get( const std::string &name )
    {
      int fd = shm_open( name.c_str(), O_RDWR, 0600 );
      if( fd < 0 )
        throw shm_error( errno );
      struct stat statbuf;
      if( fstat( fd, &statbuf ) < 0 )
        throw shm_error( errno );
      size_t size = statbuf.st_size;
      void *mem = map_shm( fd, size );
      close( fd );
      return std::make_tuple( reinterpret_cast<T*>( mem ), size );
    }

    /**
     * Helper function for creating a shared memory block and
     * constructing an array of objects of type T (constructed
     * with default constructor) within the block.
     *
     * @param name  : name of the shared memory block (shared
     *                memory object should be identified by a
     *                name of the form /somename)
     * @param count : size of the array
     * @return      : pointer to the shared memory and its size
     */
    template<typename T>
    inline static std::tuple<T*, size_t> make_array( const std::string &name, size_t count )
    {
      auto tpl = create( name, count * sizeof( T ) );
      T* arr = reinterpret_cast<T*>( std::get<0>( tpl ) );
      size_t size = std::get<1>( tpl );
      for( size_t i = 0; i < count; ++i )
        new( arr + i ) T();
      return std::make_tuple( arr, size );
    }

    /**
     * Helper function for creating a shared memory block and
     * constructing an array of objects of type T (constructed
     * with using arguments args) within the block.
     *
     * @param name  : name of the shared memory block (shared
     *                memory object should be identified by a
     *                name of the form /somename)
     * @param count : size of the array
     * @param args  : the arguments for the T constructor
     * @return      : pointer to the shared memory and its size
     */
    template<typename T, typename... Args>
    inline static std::tuple<T*, size_t> make_array( const std::string &name, size_t count, Args&&... args )
    {
      auto tpl = create( name, count * sizeof( T ) );
      T* arr = reinterpret_cast<T*>( std::get<0>( tpl ) );
      size_t size = std::get<1>( tpl );
      for( size_t i = 0; i < count; ++i )
        new( arr + i ) T( std::forward<Args...>( args... ) );
      return std::make_tuple( arr, size );
    }

    private:
      /**
       * Helper function for mapping the shared memory block
       * into user virtual address space
       *
       * @param fd   : handle to the shared memory segment
       * @param size : size of the shared memory segment
       * @return     : pointer to the shared memory
       */
      inline static void* map_shm( int fd, size_t size )
      {
        void *mem = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if( mem == MAP_FAILED )
        {
          mem = nullptr;
          throw shm_error( errno );
        }
        return mem;
      }
  };
}



#endif /* SRC_XRDSYS_XRDSYSSHMEM_HH_ */

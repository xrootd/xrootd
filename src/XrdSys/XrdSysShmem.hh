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
   * Smart pointer for encapsulating shared memory segments.
   * The destructor will automatically unmap the memory from
   * user's virtual address space.
   *
   * It is user responsibility to finalize the T object
   * (the shm_ptr does not ensure calling a destructor
   * for the object).
   */
  template<typename T>
  class shm_ptr
  {
    /// Friend factory methods
    template<typename Obj, typename... Args>
    friend shm_ptr<Obj> make_shm( const std::string&, Args... );
    template<typename Obj>
    friend shm_ptr<Obj> make_shm( const std::string& );
    template<typename Obj>
    friend shm_ptr<Obj> get_shm( const std::string& );

    public:

      /// Move constructor
      shm_ptr( shm_ptr &&mv ) : ptr( mv.ptr ), size( mv.size )
      {
        mv.ptr = nullptr;
        mv.size = 0;
      }

      /// Move assignment operator
      shm_ptr& operator=( shm_ptr &&mv )
      {
        ptr  = mv.ptr;
        size = mv.size;
        mv.ptr  = nullptr;
        mv.size = 0;
        return *this;
      }

      /// Destructor
      ~shm_ptr()
      {
        if( ptr )
          munmap( ptr, size );
      }

      /// Member access operator
      T* operator->() { return ptr; }

      /// Member access operator (const)
      const T* operator->() const { return ptr; }

      /// Dereferencing operator
      T& operator*() { return *ptr; }

      /// Dereferencing operator (const)
      const T& operator*() const { return *ptr; }

      /// @return : the raw pointer to the shared memory block
      inline void* get_raw()
      {
        return ptr;
      }

      /// @return : the size of the shared memory block
      inline size_t get_size()
      {
        return size;
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

      /**
       * Helper function for creating shared memory block
       *
       * @param name : name of the shared memory block (shared
       *               memory object should be identified by a
       *               name of the form /somename)
       * @param size : size of the shared memory segment
       * @return     : pointer to the shared memory
       */
      inline static std::tuple<void*, size_t> create_shm( const std::string &name, size_t size )
      {
        int fd = shm_open( name.c_str(), O_CREAT | O_RDWR, 0600 );
        if( fd < 0 )
          throw shm_error( errno );
        if( ftruncate( fd, size ) < 0 )
          throw shm_error( errno );
        struct stat statbuf;
        if( fstat( fd, &statbuf ) < 0 )
          throw shm_error( errno );
        size = statbuf.st_size;
        void *mem = map_shm( fd, size );
        close( fd );
        return std::make_tuple( mem, size );
      }

      /**
       * Constructor. Creates shared memory block (of size at least
       * size) identified by name argument and maps it into user
       * virtual address space and constructs within that memory
       * object of type T using arguments args.
       */
      template<typename... Args>
      shm_ptr( const std::string &name, size_t size, Args&&... args )
      {
        void *mem = nullptr;
        std::tie( mem, size ) = create_shm( name, size );
        ptr = new( mem ) T( std::forward( args... ) );
      }

      /**
       * Constructor. Creates shared memory block (of size at least
       * size) identified by name argument and maps it into user
       * virtual address space and constructs within that memory
       * object of type T using default constructor.
       */
      shm_ptr( const std::string &name, size_t size )
      {
        void *mem = nullptr;
        std::tie( mem, size ) = create_shm( name, size );
        ptr = new( mem ) T();
      }

      /**
       * Constructor for initializing from existing shared memory block.
       *
       * @param name : name of the shared memory block (shared
       *               memory object should be identified by a
       *               name of the form /somename)
       */
      shm_ptr( const std::string &name )
      {
        int fd = shm_open( name.c_str(), O_RDWR, 0600 );
        if( fd < 0 )
          throw shm_error( errno );
        struct stat statbuf;
        if( fstat( fd, &statbuf ) < 0 )
          throw shm_error( errno );
        size = statbuf.st_size;
        if( sizeof( T ) > size )
          throw shm_error( EINVAL );
        void *mem = map_shm( fd, size );
        close( fd );
        ptr = reinterpret_cast<T*>( mem );
      }

      T *ptr; //< the pointer to the shared object
      size_t size;
  };

  /**
   * Factory for creating a shared memory block.
   *
   * @param name : name of the shared memory block (shared
   *               memory object should be identified by a
   *               name of the form /somename)
   * @param args : argument to be passed to constructor of
   *               type T
   * @return     : pointer to the shared memory
   * @throws     : an instance of shm_error in case of failure
   */
  template<typename T, typename... Args>
  inline shm_ptr<T> make_shm( const std::string &name, Args&&... args )
  {
    return shm_ptr<T>( name, sizeof( T ), std::forward( args... ) );
  }

  /**
   * Factory for creating a shared memory block.
   *
   * @param name : name of the shared memory block (shared
   *               memory object should be identified by a
   *               name of the form /somename)
   * @return     : pointer to the shared memory
   * @throws     : an instance of shm_error in case of failure
   */
  template<typename T>
  inline shm_ptr<T> make_shm( const std::string &name )
  {
    return shm_ptr<T>( name, sizeof( T ) );
  }

  /**
   * Factory for getting an existing shared memory block.
   *
   * @param name : name of the shared memory block (shared
   *               memory object should be identified by a
   *               name of the form /somename)
   * @return     : pointer to the shared memory
   * @throws     : an instance of shm_error in case of failure
   */
  template<typename T>
  inline shm_ptr<T> get_shm( const std::string &name )
  {
    return shm_ptr<T>( name );
  }

  /**
   * Helper function for deleting an existing shared memory block.
   * @throws : an instance of shm_error in case of failure
   */
  inline void rm_shm( const std::string &name )
  {
    int rc = shm_unlink( name.c_str() );
    if( rc < 0 )
      throw shm_error( errno );
  }
}



#endif /* SRC_XRDSYS_XRDSYSSHMEM_HH_ */

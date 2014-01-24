//------------------------------------------------------------------------------
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
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

#ifndef __XRD_SYS_LINUX_SEMAPHORE__
#define __XRD_SYS_LINUX_SEMAPHORE__

#if defined(__linux__) && defined(HAVE_ATOMICS)

#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <exception>
#include <string>

namespace XrdSys
{
  //----------------------------------------------------------------------------
  //! Semaphore exception
  //----------------------------------------------------------------------------
  class LinuxSemaphoreError: public std::exception
  {
    public:
      LinuxSemaphoreError( const std::string &error ): pError( error ) {}
      virtual ~LinuxSemaphoreError() throw() {};

      virtual const char *what() const throw()
      {
        return pError.c_str();
      }

    private:
      std::string pError;
  };

  //----------------------------------------------------------------------------
  //! A thread safe semaphore.
  //!
  //! You might have expected the built-it thread synchronization machanisms
  //! to be thread safe, but, unfortunately, this is not the case on Linux.
  //!
  //! For more details see:
  //!
  //! https://sourceware.org/bugzilla/show_bug.cgi?id=12674
  //! https://bugzilla.redhat.com/show_bug.cgi?id=1027348
  //!
  //! This class attepmts to implement a thread safe semaphore using
  //! compiler intrinsics for atomic operations on integers and system futexes
  //! for waking and puting thread to sleep. It stores both number of waiters
  //! and the value of the semaphore in one variable that is modified atomically.
  //! It solves the races at the cost of limiting the maximal value storable in
  //! the semaphore to 20 bits and the possible number of threads waiting for
  //! the value to change to 12 bits.
  //----------------------------------------------------------------------------
  class LinuxSemaphore
  {
    public:
      //------------------------------------------------------------------------
      //! Try to acquire the semaphore without waiting
      //!
      //! @return 1 on success, 0 otherwise
      //------------------------------------------------------------------------
      inline int CondWait()
      {
        int value   = 0;
        int val     = 0;
        int waiters = 0;
        int newVal  = 0;

        //----------------------------------------------------------------------
        // We get the value of the semaphore try to atomically decrement it if
        // it's larger than 0.
        //----------------------------------------------------------------------
        while( 1 )
        {
          Unpack( pValue, value, val, waiters );
          if( val == 0 )
            return 0;
          newVal = Pack( --val, waiters );
          if( __sync_bool_compare_and_swap( pValue, value, newVal ) )
            return 1;
        }
      }

      //------------------------------------------------------------------------
      //! Acquire the semaphore, wait for it to be risen, if necessary.
      //!
      //! @throw XrdSys::LinuxSemaphoreError in case of syscall errors or
      //!        exceeding maximal value or number of waiters
      //------------------------------------------------------------------------
      inline void Wait()
      {
        //----------------------------------------------------------------------
        // Examine the state of the semaphore and atomically decrement it if
        // possible. If CondWait fails, it means that the semaphore value was 0.
        // In this case we atomically bump the number of waiters and go to sleep
        //----------------------------------------------------------------------
        while( !CondWait() )
        {
          int value   = 0;
          int val     = 0;
          int waiters = 0;
          int cancelType = 0;

          Unpack( pValue, value, val, waiters );

          if( waiters == WaitersMask )
            throw LinuxSemaphoreError( "Reached maximum number of waiters" );

          int newVal = Pack( val, ++waiters );

          //--------------------------------------------------------------------
          // We have bumped the number of waiters successfuly if neither the
          // semaphore value nor the number of waiters changed in the mean time.
          // We can safely go to sleep.
          //
          // Once the number of waiters was bumped we cannot get cancelled
          // without decrementing it.
          //--------------------------------------------------------------------
          pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, &cancelType );
          if( __sync_bool_compare_and_swap( pValue, value, newVal ) )
          {
            while( 1 )
            {
              int r = 0;

              pthread_cleanup_push( Cleanup, pValue );
              pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, 0 );

              r = syscall( SYS_futex, pValue, FUTEX_WAIT, newVal, 0, 0, 0 );

              pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, 0 );
              pthread_cleanup_pop( 0 );

              if( r == 0 )               // we've been woken up
                break;

              if( errno == EINTR )       // interrupt
                continue;

              if( errno == EWOULDBLOCK ) // futex value changed
                break;

              throw LinuxSemaphoreError( "FUTEX_WAIT syscall error" );
            }

            //------------------------------------------------------------------
            // We have been woken up, so we need to decrement the number of
            // waiters
            //------------------------------------------------------------------
            do
            {
              Unpack( pValue, value, val, waiters );
              newVal = Pack( val, --waiters );
            }
            while( !__sync_bool_compare_and_swap( pValue, value, newVal ) );
          }

          //--------------------------------------------------------------------
          // We are here if:
          // 1) we were unable to increase the number of waiters bacause the
          //    atomic changed in the mean time in another execution thread
          // 2) *pValue != newVal upon futex call, this indicates the state
          //    change in another thread
          // 3) we have been woken up by another thread
          //
          // In either of the above cases we need to re-examine the atomic and
          // decide whether we need to sleep or are free to proceed
          //--------------------------------------------------------------------
          pthread_setcanceltype( cancelType, 0 );
        }
      }

      //------------------------------------------------------------------------
      //! Unlock the semaphore
      //!
      //! @throw XrdSys::LinuxSemaphoreError in case of exceeding maximum
      //!        semaphore value
      //------------------------------------------------------------------------
      inline void Post()
      {
        int value   = 0;
        int val     = 0;
        int waiters = 0;
        int newVal  = 0;

        //----------------------------------------------------------------------
        // We atomically increment the value of the semaphore and wake one of
        // the threads that was waiting for the semaphore value to change
        //----------------------------------------------------------------------
        while( 1 )
        {
          Unpack( pValue, value, val, waiters );

          if( val == ValueMask )
            throw LinuxSemaphoreError( "Reached maximum value" );

          newVal = Pack( ++val, waiters );
          if( __sync_bool_compare_and_swap( pValue, value, newVal ) )
          {
            if( waiters )
              syscall( SYS_futex, pValue, FUTEX_WAKE, 1, 0, 0, 0 );
            return;
          }
        }
      }

      //------------------------------------------------------------------------
      //! Get the semaphore value
      //------------------------------------------------------------------------
      int GetValue() const
      {
        int value = __sync_fetch_and_add( pValue, 0 );
        return value & ValueMask;
      }

      //------------------------------------------------------------------------
      //! Construct the semaphore
      //!
      //! @param value the initial value
      //------------------------------------------------------------------------
      LinuxSemaphore( int value )
      {
        pValue = (int *)malloc(sizeof(int));
        *pValue = (value & ValueMask);
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~LinuxSemaphore()
      {
        free( pValue );
      }

    private:
      static const int ValueMask     = 0x000fffff;
      static const int WaitersOffset = 20;
      static const int WaitersMask   = 0x00000fff;

      //------------------------------------------------------------------------
      // Unpack the semaphore value
      //------------------------------------------------------------------------
      static inline void Unpack( int *sourcePtr,
                                 int &source,
                                 int &value,
                                 int &nwaiters )
      {
        source   = __sync_fetch_and_add( sourcePtr, 0 );
        value    = source & ValueMask;
        nwaiters = (source >> WaitersOffset) & WaitersMask;
      }

      //------------------------------------------------------------------------
      // Pack the semaphore value
      //------------------------------------------------------------------------
      static inline int Pack( int value, int nwaiters )
      {
        return (nwaiters << WaitersOffset) | (value & ValueMask);
      }

      //------------------------------------------------------------------------
      // Cancellation cleaner
      //------------------------------------------------------------------------
      static void Cleanup( void *param )
      {
        int *iParam = (int*)param;
        int value   = 0;
        int val     = 0;
        int waiters = 0;
        int newVal  = 0;

        do
        {
          Unpack( iParam, value, val, waiters );
          newVal = Pack( val, --waiters );
        }
        while( !__sync_bool_compare_and_swap( iParam, value, newVal ) );
      }

      int *pValue;
  };
};

#endif // __linux__ && HAVE_ATOMICS

#endif // __XRD_SYS_LINUX_SEMAPHORE__

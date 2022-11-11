//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#ifndef SRC_XRDEC_XRDECUTILITIES_HH_
#define SRC_XRDEC_XRDECUTILITIES_HH_

#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClUtils.hh"

#include <exception>
#include <memory>
#include <random>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  //! A buffer with stripe data and info on validity
  //---------------------------------------------------------------------------
  struct stripe_t
  {
    //-------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param buffer : buffer with stripe data
    //! @param valid  : true if data are valid, false otherwise
    //-------------------------------------------------------------------------
    stripe_t( char *buffer, bool valid ) : buffer( buffer ), valid( valid )
    {
    }

    char *buffer; //< buffer with stripe data
    bool  valid;  //< true if data are valid, otherwise false
  };

  //---------------------------------------------------------------------------
  //! All stripes in a block
  //---------------------------------------------------------------------------
  typedef std::vector<stripe_t> stripes_t;

  //----------------------------------------------------------------------------
  //! a buffer type
  //----------------------------------------------------------------------------
  typedef std::vector<char> buffer_t;

  //----------------------------------------------------------------------------
  //! Generic I/O exception, wraps up XrdCl::XRootDStatus (@see XRootDStatus)
  //----------------------------------------------------------------------------
  class IOError : public std::exception
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param st : status
      //------------------------------------------------------------------------
      IOError( const XrdCl::XRootDStatus &st ) noexcept : st( st ), msg( st.ToString() )
      {
      }

      //------------------------------------------------------------------------
      //! Copy constructor
      //------------------------------------------------------------------------
      IOError( const IOError &err ) noexcept : st( err.st ), msg( err.st.ToString() )
      {
      }

      //------------------------------------------------------------------------
      //! Assigment operator
      //------------------------------------------------------------------------
      IOError& operator=( const IOError &err ) noexcept
      {
        st = err.st;
        msg = err.st.ToString();
        return *this;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~IOError()
      {
      }

      //------------------------------------------------------------------------
      //! overloaded @see std::exception
      //------------------------------------------------------------------------
      virtual const char* what() const noexcept
      {
        return msg.c_str();
      }

      //------------------------------------------------------------------------
      //! @return : the status
      //------------------------------------------------------------------------
      const XrdCl::XRootDStatus& Status() const
      {
        return st;
      }

      enum
      {
        ioTooManyErrors
      };

    private:

      //------------------------------------------------------------------------
      //! The status object
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus st;

      //------------------------------------------------------------------------
      //! The error message
      //------------------------------------------------------------------------
      std::string msg;
  };

  //---------------------------------------------------------------------------
  //! A utility function for scheduling read operation handler
  //!
  //! @param offset  : offset of the read
  //! @param size    : number of bytes read
  //! @param buffer  : buffer with the data read
  //! @param handler : user callback
  //---------------------------------------------------------------------------
  void ScheduleHandler( uint64_t offset, uint32_t size, void *buffer, XrdCl::ResponseHandler *handler );

  //---------------------------------------------------------------------------
  //! A utility function for scheduling an operation handler
  //!
  //! @param handler : user callback
  //! @param st      : operation status
  //---------------------------------------------------------------------------
  void ScheduleHandler( XrdCl::ResponseHandler *handler, const XrdCl::XRootDStatus &st = XrdCl::XRootDStatus() );


  //---------------------------------------------------------------------------
  // A class implementing synchronous queue
  //---------------------------------------------------------------------------
  template<typename Element>
  struct sync_queue
  {
    //-------------------------------------------------------------------------
    // An internal exception used for interrupting the `dequeue` method
    //-------------------------------------------------------------------------
    struct wait_interrupted{ };

    //-------------------------------------------------------------------------
    // Default constructor
    //-------------------------------------------------------------------------
    sync_queue() : interrupted( false )
    {
    }

    //-------------------------------------------------------------------------
    // Enqueue new element into the queue
    //-------------------------------------------------------------------------
    inline void enqueue( Element && element )
    {
      std::unique_lock<std::mutex> lck( mtx );
      elements.push( std::move( element ) );
      cv.notify_all();
    }

    //-------------------------------------------------------------------------
    // Dequeue an element from the front of the queue
    // Note: if the queue is empty blocks until a new element is enqueued
    //-------------------------------------------------------------------------
    inline Element dequeue()
    {
      std::unique_lock<std::mutex> lck( mtx );
      while( elements.empty() )
      {
        cv.wait( lck );
        if( interrupted ) throw wait_interrupted();
      }
      Element element = std::move( elements.front() );
      elements.pop();
      return element;
    }

    //-------------------------------------------------------------------------
    // Dequeue an element from the front of the queue
    // Note: if the queue is empty returns false, true otherwise
    //-------------------------------------------------------------------------
    inline bool dequeue( Element &e )
    {
      std::unique_lock<std::mutex> lck( mtx );
      if( elements.empty() ) return false;
      e = std::move( elements.front() );
      elements.pop();
      return true;
    }

    //-------------------------------------------------------------------------
    // Checks if the queue is empty
    //-------------------------------------------------------------------------
    bool empty()
    {
      std::unique_lock<std::mutex> lck( mtx );
      return elements.empty();
    }

    //-------------------------------------------------------------------------
    // Interrupt all waiting `dequeue` routines
    //-------------------------------------------------------------------------
    inline void interrupt()
    {
      interrupted = true;
      cv.notify_all();
    }

    private:
      std::queue<Element>     elements;    //< the queue itself
      std::mutex              mtx;         //< mutex guarding the queue
      std::condition_variable cv;
      std::atomic<bool>       interrupted; //< a flag, true if all `dequeue` routines
                                           //< should be interrupted
  };

  //---------------------------------------------------------------------------
  // Extract the block ID from the chunk file name
  //---------------------------------------------------------------------------
  inline static size_t fntoblk( const std::string &fn )
  {
    size_t end = fn.rfind( '.' );
    size_t begin = fn.rfind( '.', end - 1 ) + 1;
    size_t len = end - begin;
    return std::stoul( fn.substr( begin,  len ) );
  }
}

#endif /* SRC_XRDEC_XRDECUTILITIES_HH_ */

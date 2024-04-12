/*
 * XrdClOperationTimeout.hh
 *
 *  Created on: 4 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLOPERATIONTIMEOUT_HH_
#define SRC_XRDCL_XRDCLOPERATIONTIMEOUT_HH_

#include <cstdint>
#include <ctime>
#include <exception>

namespace XrdCl
{
  class operation_expired : public std::exception {};

  class Timeout
  {
    public:

      Timeout(): timeout( 0 ), start( 0 )
      {
      }

      Timeout( time_t timeout ): timeout( timeout ), start( time( 0 ) )
      {
      }

      Timeout& operator=( const Timeout &to )
      {
        timeout = to.timeout;
        start   = to.start;
        return *this;
      }

      Timeout( const Timeout &to ) : timeout( to.timeout ), start( to.start )
      {
      }

      operator time_t() const
      {
        if( !timeout ) return 0;
        time_t elapsed = time( 0 ) - start;
        if( timeout < elapsed) throw operation_expired();
        return timeout - elapsed;
      }

    private:

      time_t   timeout;
      time_t   start;
  };

}

#endif /* SRC_XRDCL_XRDCLOPERATIONTIMEOUT_HH_ */

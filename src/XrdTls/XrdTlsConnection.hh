//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <simonm@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef __XRD_TLS_IO_HH__
#define __XRD_TLS_IO_HH__

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include "XrdTls/XrdTlsCtx.hh"

namespace XrdTls
{
  //----------------------------------------------------------------------------
  //! Socket wrapper for TLS I/O
  //----------------------------------------------------------------------------
  class Connection
  {
    public:

      enum Mode
      {
        TLS_CLIENT,
        TLS_SERVER
      };

      //------------------------------------------------------------------------
      //! Constructor - creates async TLS I/O wrapper for given socket
      //! file descriptor
      //------------------------------------------------------------------------
      Connection( int sfd, Mode mode );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Connection();

      //------------------------------------------------------------------------
      //! Accept an incoming TLS connection
      //------------------------------------------------------------------------
      int Accept();

      //------------------------------------------------------------------------
      //! Establish a TLS connection
      //------------------------------------------------------------------------
      int Connect();

      //------------------------------------------------------------------------
      //! Read from the TLS connection
      //! (If necessary, will establish a TLS/SSL session.)
      //------------------------------------------------------------------------
      int Read( char *buffer, size_t size, int &bytesRead );

      //------------------------------------------------------------------------
      //! Write to the TLS connection
      //! (If necessary, will establish a TLS/SSL session.)
      //------------------------------------------------------------------------
      int Write( char *buffer, size_t size, int &bytesWritten );

      //------------------------------------------------------------------------
      //! @return  :  true if the TLS/SSL session is not established yet,
      //!             false otherwise
      //------------------------------------------------------------------------
      bool NeedHandShake()
      {
        return !hsDone;
      }

      //------------------------------------------------------------------------
      //! Conversion to native OpenSSL connection object
      //!
      //! @return : SSL connection object
      //------------------------------------------------------------------------
      operator SSL*()
      {
        return ssl;
      }

    private:

      //------------------------------------------------------------------------
      //! The TSL/SSL object.
      //------------------------------------------------------------------------
      SSL  *ssl;

      //------------------------------------------------------------------------
      //! The I/O interface.
      //------------------------------------------------------------------------
      BIO  *sbio;

      //------------------------------------------------------------------------
      //! True if TSL/SSL handshake has been done, flase otherwise.
      //------------------------------------------------------------------------
      bool  hsDone;
  };
}

#endif // __XRD_CL_TLS_HH__


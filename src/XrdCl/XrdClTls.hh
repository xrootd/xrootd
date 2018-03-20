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

#ifndef __XRD_CL_TLS_HH__
#define __XRD_CL_TLS_HH__

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include "XrdCl/XrdClStatus.hh"
#include "XrdTls/XrdTlsConnection.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! TLS layer for socket connection
  //----------------------------------------------------------------------------
  class Tls
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor - creates async TLS layer for given socker file descriptor
      //------------------------------------------------------------------------
      Tls( int sfd );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Tls();

      //------------------------------------------------------------------------
      //! Read through the TLS layer from the socket
      //! If necessary, will establish a TLS/SSL session.
      //------------------------------------------------------------------------
      Status Read( char *buffer, size_t size, int &bytesRead );

      //------------------------------------------------------------------------
      //! Write through the TLS layer to the socket
      //! If necessary, will establish a TLS/SSL session.
      //------------------------------------------------------------------------
      Status Write( char *buffer, size_t size, int &bytesWritten );

      //------------------------------------------------------------------------
      //! @return  :  true if the TLS/SSL session is not established yet,
      //!             false otherwise
      //------------------------------------------------------------------------
      bool NeedHandShake()
      {
        return io.NeedHandShake();
      }

    private:

      //------------------------------------------------------------------------
      //! Translate OPEN SSL error code into XRootD Status
      //------------------------------------------------------------------------
      Status ToStatus( int rc );

      //------------------------------------------------------------------------
      //! The TSL I/O wrapper
      //------------------------------------------------------------------------
      XrdTls::Connection io;
  };

  //----------------------------------------------------------------------------
  //! Read helper for TLS layer
  //!
  //! @param tls       : the TLS object
  //! @param buffer    : the sink for the data
  //! @param size      : size of the sink
  //! @param bytesRead : number of bytes actually written into the sink
  //! @return          : SSL_ERROR_NONE       : ( stOK )
  //!                    SSL_ERROR_WANT_WRITE : ( stOK, suRetry )
  //!                    SSL_ERROR_WANT_READ  : ( stOK, suRetry )
  //!                    otherwise            : ( stError, errTlsError )
  //----------------------------------------------------------------------------
  Status ReadFrom( Tls *tls, char *buffer, size_t size, int &bytesRead );
}

#endif // __XRD_CL_TLS_HH__


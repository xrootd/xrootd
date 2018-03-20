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

#ifndef __XRD_TLS_HH__
#define __XRD_TLS_HH__

#include <openssl/ssl.h>
#include <string>


namespace XrdTls
{
  //----------------------------------------------------------------------------
  //! TLS/SSL exception
  //----------------------------------------------------------------------------
  struct Exception
  {
      enum Error
      {
        ALLOC_ERR,
        CERT_ERR,
        KEY_ERR,
        CERT_KEY_MISMATCH,
        ALREADYDONE
      };

      Exception( const char* file, int line, Error code ) : file( file ), line( line ), error( code )
      {

      }

      // source file where the exception was created
      const std::string file;
      // line at which the exception was created
      const int         line;
      // the TLS/SSL error code
      Error             error;
  };

//----------------------------------------------------------------------------
//! Utility macro for creating TLS Exceptions
//----------------------------------------------------------------------------
#define make_tlserr( code ) Exception( __FILE__, __LINE__, code )

  //----------------------------------------------------------------------------
  //! TLS/SSL context (singleton)
  //----------------------------------------------------------------------------
  class Context
  {
    public:

      //------------------------------------------------------------------------
      //! Initialize the TLS context
      //------------------------------------------------------------------------
      static void Create( const std::string &cert = "",
                          const std::string &key  = "" );

      //------------------------------------------------------------------------
      //! @return : instance of TLS/SSL context
      //------------------------------------------------------------------------
      static Context& Instance()
      {
        if( !instance ) Create();

        return *instance;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Context();

      //------------------------------------------------------------------------
      //! Conversion to SSL_CTX
      //------------------------------------------------------------------------
      operator SSL_CTX*();

    private:

      //------------------------------------------------------------------------
      //! Private constructor
      //!
      //! Initializes TLS/SSL
      //------------------------------------------------------------------------
      Context( const std::string &cert, const std::string & key );

      Context( const Context  &ctx ) = delete;
      Context(       Context &&ctx ) = delete;
      Context& operator=( const Context  &ctx ) = delete;
      Context& operator=(       Context &&ctx ) = delete;

      static Context *instance;
      SSL_CTX        *ctx;
  };
}

#endif // __XRD_TLS_HH__


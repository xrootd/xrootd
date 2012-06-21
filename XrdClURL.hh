//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_URL_HH__
#define __XRD_CL_URL_HH__

#include <string>
#include <map>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! URL representation
  //----------------------------------------------------------------------------
  class URL
  {
    public:
      typedef std::map<std::string, std::string> ParamsMap; //!< Map of get
                                                            //!< params

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url  an url in format:
      //!             user:password\@host:port/path?param1=x&param2=y
      //------------------------------------------------------------------------
      URL( const std::string &url );

      //------------------------------------------------------------------------
      //! Is the url valide
      //------------------------------------------------------------------------
      bool IsValid() const
      {
        return pIsValid;
      }

      //------------------------------------------------------------------------
      //! Get the URL
      //------------------------------------------------------------------------
      std::string GetURL() const;

      //------------------------------------------------------------------------
      //! Get the host part of the URL (user:password\@host:port)
      //------------------------------------------------------------------------
      std::string GetHostId() const;

      //------------------------------------------------------------------------
      //! Get the protocol
      //------------------------------------------------------------------------
      const std::string &GetProtocol() const
      {
        return pProtocol;
      }

      //------------------------------------------------------------------------
      //! Set protocol
      //------------------------------------------------------------------------
      void SetProtocol( const std::string &protocol )
      {
        pProtocol = protocol;
      }

      //------------------------------------------------------------------------
      //! Get the username
      //------------------------------------------------------------------------
      const std::string &GetUserName() const
      {
        return pUserName;
      }

      //------------------------------------------------------------------------
      //! Set the username
      //------------------------------------------------------------------------
      void SetUserName( const std::string &userName )
      {
        pUserName = userName;
      }

      //------------------------------------------------------------------------
      //! Get the password
      //------------------------------------------------------------------------
      const std::string &GetPassword() const
      {
        return pPassword;
      }

      //------------------------------------------------------------------------
      //! Set the password
      //------------------------------------------------------------------------
      void SetPassword( const std::string &password )
      {
        pPassword = password;
      }

      //------------------------------------------------------------------------
      //! Get the name of the target host
      //------------------------------------------------------------------------
      const std::string &GetHostName() const
      {
        return pHostName;
      }

      //------------------------------------------------------------------------
      //! Set the host name
      //------------------------------------------------------------------------
      void SetHostName( const std::string &hostName )
      {
        pHostName = hostName;
      }

      //------------------------------------------------------------------------
      //! Get the target port
      //------------------------------------------------------------------------
      int GetPort() const
      {
        return pPort;
      }

      //------------------------------------------------------------------------
      // Set port
      //------------------------------------------------------------------------
      void SetPort( int port )
      {
        pPort = port;
      }

      //------------------------------------------------------------------------
      //! Get the path
      //------------------------------------------------------------------------
      const std::string &GetPath() const
      {
        return pPath;
      }

      //------------------------------------------------------------------------
      //! Set the path
      //------------------------------------------------------------------------
      void SetPath( const std::string &path )
      {
        pPath = path;
      }

      //------------------------------------------------------------------------
      //! Get the path with params
      //------------------------------------------------------------------------
      std::string GetPathWithParams() const;

      //------------------------------------------------------------------------
      //! Get the URL params
      //------------------------------------------------------------------------
      const ParamsMap &GetParams() const
      {
        return pParams;
      }

      //------------------------------------------------------------------------
      //! Get the URL params
      //------------------------------------------------------------------------
      ParamsMap &GetParams()
      {
        return pParams;
      }

    private:
      void ParseUrl( const std::string &url );
      bool ParseHostInfo( const std::string hhostInfo );
      bool ParsePath( const std::string &path );
      bool        pIsValid;
      std::string pHostId;
      std::string pProtocol;
      std::string pUserName;
      std::string pPassword;
      std::string pHostName;
      int         pPort;
      std::string pPath;
      ParamsMap   pParams;
  };
}

#endif // __XRD_CL_URL_HH__

//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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
      //! Default constructor
      //------------------------------------------------------------------------
      URL();

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url  an url in format:
      //!             protocol://user:password\@host:port/path?param1=x&param2=y
      //------------------------------------------------------------------------
      URL( const std::string &url );

      //------------------------------------------------------------------------
      //! Is the url valid
      //------------------------------------------------------------------------
      bool IsValid() const;

      //------------------------------------------------------------------------
      //! Get the URL
      //------------------------------------------------------------------------
      std::string GetURL() const
      {
        return pURL;
      }

      //------------------------------------------------------------------------
      //! Get the host part of the URL (user:password\@host:port)
      //------------------------------------------------------------------------
      std::string GetHostId() const
      {
        return pHostId;
      }

      //------------------------------------------------------------------------
      //! Get location (protocol://host:port/path)
      //------------------------------------------------------------------------
      std::string GetLocation() const;

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
        ComputeURL();
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
        ComputeHostId();
        ComputeURL();
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
        ComputeURL();
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
        ComputeHostId();
        ComputeURL();
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
        ComputeHostId();
        ComputeURL();
      }

      //------------------------------------------------------------------------
      // Set host and port
      //------------------------------------------------------------------------
      void SetHostPort( const std::string &hostName, int port )
      {
        pHostName = hostName;
        pPort     = port;
        ComputeHostId();
        ComputeURL();
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
        ComputeURL();
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
      //! Get the URL params as string
      //------------------------------------------------------------------------
      std::string GetParamsAsString() const;

      //------------------------------------------------------------------------
      //! Set params
      //------------------------------------------------------------------------
      void SetParams( const std::string &params );

      //------------------------------------------------------------------------
      //! Set params
      //------------------------------------------------------------------------
      void SetParams( const ParamsMap &params )
      {
        pParams = params;
        ComputeURL();
      }

      //------------------------------------------------------------------------
      //! Parse a string and fill the URL fields
      //------------------------------------------------------------------------
      bool FromString( const std::string &url );

      //------------------------------------------------------------------------
      //! Clear the url
      //------------------------------------------------------------------------
      void Clear();

    private:
      bool ParseHostInfo( const std::string hhostInfo );
      bool ParsePath( const std::string &path );
      void ComputeHostId();
      void ComputeURL();
      std::string pHostId;
      std::string pProtocol;
      std::string pUserName;
      std::string pPassword;
      std::string pHostName;
      int         pPort;
      std::string pPath;
      ParamsMap   pParams;
      std::string pURL;

  };
}

#endif // __XRD_CL_URL_HH__

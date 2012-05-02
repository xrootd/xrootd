//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
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
      //! @param port a port specification, if needs to be suppled externally
      //------------------------------------------------------------------------
      URL( const std::string &url, int port = 1094 );

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
      const std::string &GetURL() const
      {
        return pUrl;
      }

      //------------------------------------------------------------------------
      //! Get the host part of the URL (user:password\@host:port)
      //------------------------------------------------------------------------
      const std::string &GetHostId() const
      {
        return pHostId;
      }

      //------------------------------------------------------------------------
      //! Get the protocol
      //------------------------------------------------------------------------
      const std::string &GetProtocol() const
      {
        return pProtocol;
      }

      //------------------------------------------------------------------------
      //! Get the username
      //------------------------------------------------------------------------
      const std::string &GetUserName() const
      {
        return pUserName;
      }

      //------------------------------------------------------------------------
      //! Get the password
      //------------------------------------------------------------------------
      const std::string &GetPassword() const
      {
        return pPassword;
      }

      //------------------------------------------------------------------------
      //! Get the name of the target host
      //------------------------------------------------------------------------
      const std::string &GetHostName() const
      {
        return pHostName;
      }

      //------------------------------------------------------------------------
      //! Get the target port
      //------------------------------------------------------------------------
      int GetPort() const
      {
        return pPort;
      }

      //------------------------------------------------------------------------
      //! Get the path
      //------------------------------------------------------------------------
      const std::string &GetPath() const
      {
        return pPath;
      }

      //------------------------------------------------------------------------
      //! Get the path with params
      //------------------------------------------------------------------------
      const std::string &GetPathWithParams() const
      {
        return pPathWithParams;
      }

      //------------------------------------------------------------------------
      //! Get the URL params
      //------------------------------------------------------------------------
      const ParamsMap &GetParams() const
      {
        return pParams;
      }

    private:
      void ParseUrl();
      bool        pIsValid;
      std::string pUrl;
      std::string pHostId;
      std::string pProtocol;
      std::string pUserName;
      std::string pPassword;
      std::string pHostName;
      int         pPort;
      std::string pPath;
      std::string pPathWithParams;
      ParamsMap   pParams;
  };
}

#endif // __XRD_CL_URL_HH__

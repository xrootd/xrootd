/*
 * XrdClMetalinkRedirector.hh
 *
 *  Created on: May 2, 2016
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLMETALINKREDIRECTOR_HH_
#define SRC_XRDCL_XRDCLMETALINKREDIRECTOR_HH_

#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"

#include <string>
#include <list>
#include <map>


class XrdOucFileInfo;

namespace XrdCl
{

class File;
class Message;
class Stream;

//----------------------------------------------------------------------------
//! An abstraction representing a virtual
//! redirector based on a metalink file
//----------------------------------------------------------------------------
class MetalinkRedirector : public VirtualRedirector
{
    friend class MetalinkOpenHandler;
    friend class MetalinkReadHandler;

  public:
    //----------------------------------------------------------------------------
    //! Constructor
    //! @param url         : URL to the metalink file
    //! @param userHandler : the response handler provided by end user
    //----------------------------------------------------------------------------
    MetalinkRedirector( const std::string &url );

    //----------------------------------------------------------------------------
    //! Destructor
    //----------------------------------------------------------------------------
    virtual ~MetalinkRedirector();

    //----------------------------------------------------------------------------
    //! Initializes the object with the content of the metalink file
    //----------------------------------------------------------------------------
    XRootDStatus Load( ResponseHandler *userHandler );

    //----------------------------------------------------------------------------
    //! Creates an instant redirect response for the given message
    //! or an error response if there are no more replicas to try.
    //! The virtual response is being handled by the given stream.
    //----------------------------------------------------------------------------
    XRootDStatus HandleRequest( Message *msg, Stream *stream );

    //----------------------------------------------------------------------------
    //! Gets the file name as specified in the metalink
    //----------------------------------------------------------------------------
    std::string GetTargetName() const
    {
      return pTarget;
    }

    //----------------------------------------------------------------------------
    //! Returns the checksum of the given type if specified
    //! in the metalink file, or an empty string otherwise
    //----------------------------------------------------------------------------
    std::string GetCheckSum( const std::string &type ) const
    {
      CksumMap::const_iterator it = pChecksums.find( type );
      if( it == pChecksums.end() ) return std::string();
      return type + ":" + it->second;
    }

    //----------------------------------------------------------------------------
    //! Returns the file size if specified in the metalink file,
    //! otherwise a negative number
    //----------------------------------------------------------------------------
    long long GetSize() const
    {
      return pFileSize;
    }

    //----------------------------------------------------------------------------
    //! Returns a vector with replicas as given in the meatlink file
    //----------------------------------------------------------------------------
    const std::vector<std::string>& GetReplicas()
    {
      return pReplicas;
    }

  private:

    //----------------------------------------------------------------------------
    //! Parses the metalink file
    //! @param metalink : the content of the metalink file
    //----------------------------------------------------------------------------
    XRootDStatus Parse( const std::string &metalink );

    //----------------------------------------------------------------------------
    //! Finalize the initialization process:
    //! - mark as ready
    //! - setup the status
    //! - and handle pending redirects
    //----------------------------------------------------------------------------
    void FinalizeInitialization( const XRootDStatus &status = XRootDStatus() );

    //----------------------------------------------------------------------------
    //! Generates redirect response for the given request
    //----------------------------------------------------------------------------
    Message* GetResponse( const Message *msg ) const;

    //----------------------------------------------------------------------------
    //! Generates error response for the given request
    //----------------------------------------------------------------------------
    Message* GetErrorMsg( const Message *msg, const std::string &errMsg, XErrorCode code ) const;

    //----------------------------------------------------------------------------
    //! Initializes checksum map
    //----------------------------------------------------------------------------
    void InitCksum( XrdOucFileInfo **fileInfos );

    //----------------------------------------------------------------------------
    //! Initializes replica list
    //----------------------------------------------------------------------------
    void InitReplicas( XrdOucFileInfo **fileInfos );

    //----------------------------------------------------------------------------
    //! Get the next replica for the given message
    //----------------------------------------------------------------------------
    XRootDStatus GetReplica( const Message *msg, std::string &replica ) const;

    //----------------------------------------------------------------------------
    //! Extracts an element from URL cgi
    //----------------------------------------------------------------------------
    XRootDStatus GetCgiInfo( const Message *msg, const std::string &key, std::string &out ) const;

    //----------------------------------------------------------------------------
    //! Loads a local Metalink file
    //----------------------------------------------------------------------------
    XRootDStatus LoadLocalFile( ResponseHandler *userHandler );

    //----------------------------------------------------------------------------
    //! Checks if the given URL points to a local file
    //! (by convention we assume that a file is local
    //! if the host name equals to 'localfile')
    //----------------------------------------------------------------------------
    static bool IsLocalFile( const std::string &url );


    typedef std::list< std::pair<const Message*, Stream*> > RedirectList;
    typedef std::map<std::string, std::string>              CksumMap;
    typedef std::vector<std::string>                        ReplicaList;

    RedirectList     pPendingRedirects;
    std::string      pUrl;
    File            *pFile;
    CksumMap         pChecksums;
    ReplicaList      pReplicas;
    bool             pReady;
    XRootDStatus     pStatus;
    std::string      pTarget;
    long long        pFileSize;

    XrdSysMutex      pMutex;

    static const std::string LocalFile;

};

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLMETALINKREDIRECTOR_HH_ */

/*
 * XrdClRedirectorRegister.hh
 *
 *  Created on: May 23, 2016
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLREDIRECTORREGISTRY_HH_
#define SRC_XRDCL_XRDCLREDIRECTORREGISTRY_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <string>
#include <map>

namespace XrdCl
{

class Message;
class Stream;

//--------------------------------------------------------------------------------
//! An interface for metadata redirectors.
//--------------------------------------------------------------------------------
class VirtualRedirector
{
  public:
    //----------------------------------------------------------------------------
    //! Destructor.
    //----------------------------------------------------------------------------
    virtual ~VirtualRedirector(){}

    //----------------------------------------------------------------------------
    //! Creates an instant redirect response for the given message
    //! or an error response if there are no more replicas to try.
    //! The virtual response is being handled by the given stream.
    //----------------------------------------------------------------------------
    virtual XRootDStatus Redirect( Message *msg, Stream *stream ) = 0;

    //----------------------------------------------------------------------------
    //! Initializes the object with the content of the metalink file
    //----------------------------------------------------------------------------
    virtual XRootDStatus Load( ResponseHandler *userHandler ) = 0;

    //----------------------------------------------------------------------------
    //! Gets the file name as specified in the metalink
    //----------------------------------------------------------------------------
    virtual std::string GetTargetName() const = 0;

    //----------------------------------------------------------------------------
    //! Returns the checksum of the given type if specified
    //! in the metalink file, or an empty string otherwise
    //----------------------------------------------------------------------------
    virtual std::string GetCheckSum( const std::string &type ) const = 0;

    //----------------------------------------------------------------------------
    //! Returns the file size as specified in the metalink,
    //! or a negative number if size was not specified
    //----------------------------------------------------------------------------
    virtual long long GetSize() const = 0;
};

//--------------------------------------------------------------------------------
//! Singleton access to URL to virtual redirector mapping.
//--------------------------------------------------------------------------------
class RedirectorRegistry
{

  public:

    //----------------------------------------------------------------------------
    //! Returns reference to the single instance.
    //----------------------------------------------------------------------------
    static RedirectorRegistry& Instance();

    //----------------------------------------------------------------------------
    //! Destructor
    //----------------------------------------------------------------------------
    ~RedirectorRegistry();

    //----------------------------------------------------------------------------
    //! Creates a new virtual redirector and registers it (async).
    //! The response handler will be called only in case of failure!
    //----------------------------------------------------------------------------
    XRootDStatus Register( const URL &url, ResponseHandler *handler );

    //----------------------------------------------------------------------------
    //! Creates a new virtual redirector and registers it (async).
    //----------------------------------------------------------------------------
    XRootDStatus Register( const URL &url );

    //----------------------------------------------------------------------------
    //! Erase a virtual redirector for the registry.
    //----------------------------------------------------------------------------
    void Erase( const URL &url );

    //----------------------------------------------------------------------------
    //! Get a virtual redirector associated with the given URL.
    //----------------------------------------------------------------------------
    VirtualRedirector* Get( const URL &url ) const;

  private:

    //----------------------------------------------------------------------------
    //! Register implementation.
    //----------------------------------------------------------------------------
    XRootDStatus RegisterImpl( const URL &url, ResponseHandler *handler );

    //----------------------------------------------------------------------------
    // Constructor (private!).
    //----------------------------------------------------------------------------
    RedirectorRegistry() {}

    //----------------------------------------------------------------------------
    // Copy constructor (private!).
    //----------------------------------------------------------------------------
    RedirectorRegistry( const RedirectorRegistry & );

    //----------------------------------------------------------------------------
    // Assignment operator (private!).
    //----------------------------------------------------------------------------
    RedirectorRegistry& operator=( const RedirectorRegistry & );

    std::map< std::string, VirtualRedirector* > pRegistry;

    mutable XrdSysMutex pMutex;
};

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLREDIRECTORREGISTRY_HH_ */

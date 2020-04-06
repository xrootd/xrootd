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
#include "XrdCl/XrdClJobManager.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <string>
#include <vector>
#include <map>

namespace XrdCl
{

class Message;
class IncomingMsgHandler;
class OutgoingMsgHandler;

//--------------------------------------------------------------------------------
//! A job class for redirect handling in the thread-pool
//--------------------------------------------------------------------------------
class RedirectJob: public Job
{
  public:
    //------------------------------------------------------------------------
    //! Constructor
    //------------------------------------------------------------------------
    RedirectJob( IncomingMsgHandler *handler ) : pHandler( handler )
    {
    }

    //------------------------------------------------------------------------
    //! Destructor
    //------------------------------------------------------------------------
    virtual ~RedirectJob()
    {
    }

    //------------------------------------------------------------------------
    //! Run the user handler
    //------------------------------------------------------------------------
    virtual void Run( void *arg );

  private:
    IncomingMsgHandler *pHandler;
};

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
    //! The virtual response is being handled by the given handler
    //! in the thread-pool.
    //----------------------------------------------------------------------------
    virtual XRootDStatus HandleRequest( const Message *msg,
                                        IncomingMsgHandler *handler ) = 0;

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
    //! Returns the default checksum type (the first one given in the metalink),
    //! if no checksum is available returns an empty string
    //----------------------------------------------------------------------------
    virtual std::vector<std::string> GetSupportedCheckSums() const = 0;

    //----------------------------------------------------------------------------
    //! Returns the file size as specified in the metalink,
    //! or a negative number if size was not specified
    //----------------------------------------------------------------------------
    virtual long long GetSize() const = 0;

    //----------------------------------------------------------------------------
    //! Returns a vector with replicas as given in the meatlink file
    //----------------------------------------------------------------------------
    virtual const std::vector<std::string>& GetReplicas() = 0;

    //----------------------------------------------------------------------------
    //! Count how many replicas do we have left to try for given request
    //----------------------------------------------------------------------------
    virtual int Count( Message *req ) const = 0;
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
    //----------------------------------------------------------------------------
    XRootDStatus Register( const URL &url );

    //----------------------------------------------------------------------------
    //! Creates a new virtual redirector and registers it (sync).
    //----------------------------------------------------------------------------
    XRootDStatus RegisterAndWait( const URL &url );

    //----------------------------------------------------------------------------
    //! Get a virtual redirector associated with the given URL.
    //----------------------------------------------------------------------------
    VirtualRedirector* Get( const URL &url ) const;

    //----------------------------------------------------------------------------
    //! Release the virtual redirector associated with the given URL
    //----------------------------------------------------------------------------
    void Release( const URL &url );

  private:

    typedef std::map< std::string, std::pair<VirtualRedirector*, size_t> > RedirectorMap;

    //----------------------------------------------------------------------------
    //! Register implementation.
    //----------------------------------------------------------------------------
    XRootDStatus RegisterImpl( const URL &url, ResponseHandler *handler );

    //----------------------------------------------------------------------------
    //! Convert the old convention for accessing local metalink files:
    //!   root://localfile//path/metalink.meta4
    //! into:
    //!   file://localhost/path/metalink.meta4
    //----------------------------------------------------------------------------
    static URL ConvertLocalfile( const URL &url );

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

    RedirectorMap pRegistry;

    mutable XrdSysMutex pMutex;
};

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLREDIRECTORREGISTRY_HH_ */

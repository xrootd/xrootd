//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFileStateHandler.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClForkHandler.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClFileTimer.hh"
#include "XrdCl/XrdClResponseJob.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClAnyObject.hh"

#ifdef WITH_XRDEC
#include "XrdCl/XrdClEcHandler.hh"
#endif

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"

#include "XrdSys/XrdSysKernelBuffer.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <sstream>
#include <memory>
#include <numeric>
#include <sys/time.h>
#include <uuid/uuid.h>
#include <mutex>

namespace
{
  //----------------------------------------------------------------------------
  // Helper callback for handling PgRead responses
  //----------------------------------------------------------------------------
  class PgReadHandler : public XrdCl::ResponseHandler
  {
      friend class PgReadRetryHandler;

    public:

      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      PgReadHandler( XrdCl::FileStateHandler *stateHandler,
                     XrdCl::ResponseHandler  *userHandler,
                     uint64_t                 orgOffset ) : stateHandler( stateHandler ),
                                                            userHandler( userHandler ),
                                                            orgOffset( orgOffset ),
                                                            maincall( true ),
                                                            retrycnt( 0 ),
                                                            nbrepair( 0 )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *response,
                                    XrdCl::HostList     *hostList )
      {
        using namespace XrdCl;

        std::unique_lock<std::mutex> lck( mtx );

        if( !maincall )
        {
          //--------------------------------------------------------------------
          // We are serving PgRead retry request
          //--------------------------------------------------------------------
          --retrycnt;
          if( !status->IsOK() )
            st.reset( status );
          else
          {
            delete status; // by convention other args are null (see PgReadRetryHandler)
            ++nbrepair;    // update number of repaired pages
          }

          if( retrycnt == 0 )
          {
            //------------------------------------------------------------------
            // All retries came back
            //------------------------------------------------------------------
            if( st->IsOK() )
            {
              PageInfo &pginf = XrdCl::To<PageInfo>( *resp );
              pginf.SetNbRepair( nbrepair );
              userHandler->HandleResponseWithHosts( st.release(), resp.release(), hosts.release() );
            }
            else
              userHandler->HandleResponseWithHosts( st.release(), 0, 0 );
            lck.unlock();
            delete this;
          }

          return;
        }

        //----------------------------------------------------------------------
        // We are serving main PgRead request
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          //--------------------------------------------------------------------
          // The main PgRead request has failed
          //--------------------------------------------------------------------
          userHandler->HandleResponseWithHosts( status, response, hostList );
          lck.unlock();
          delete this;
          return;
        }

        maincall = false;

        //----------------------------------------------------------------------
        // Do the integrity check
        //----------------------------------------------------------------------
        PageInfo *pginf = 0;
        response->Get( pginf );

        uint64_t               pgoff     = pginf->GetOffset();
        uint32_t               bytesRead = pginf->GetLength();
        std::vector<uint32_t> &cksums    = pginf->GetCksums();
        char                  *buffer    = reinterpret_cast<char*>( pginf->GetBuffer() );
        size_t                 nbpages   = XrdOucPgrwUtils::csNum( pgoff, bytesRead );
        uint32_t               pgsize    = XrdSys::PageSize - pgoff % XrdSys::PageSize;
        if( pgsize > bytesRead ) pgsize = bytesRead;

        for( size_t pgnb = 0; pgnb < nbpages; ++pgnb )
        {
          uint32_t crcval = XrdOucCRC::Calc32C( buffer, pgsize );
          if( crcval != cksums[pgnb] )
          {
            Log *log = DefaultEnv::GetLog();
            log->Info( FileMsg, "[0x%x@%s] Received corrupted page, will retry page #%d.",
                        this, stateHandler->pFileUrl->GetURL().c_str(), pgnb );

            XRootDStatus st = stateHandler->PgReadRetry( pgoff, pgsize, pgnb, buffer, this, 0 );
            if( !st.IsOK())
            {
              *status = st; // the reason for this failure
              break;
            }
            ++retrycnt; // update the retry counter
          }

          bytesRead -= pgsize;
          buffer    += pgsize;
          pgoff     += pgsize;
          pgsize     = XrdSys::PageSize;
          if( pgsize > bytesRead ) pgsize = bytesRead;
        }


        if( retrycnt == 0 )
        {
          //--------------------------------------------------------------------
          // All went well!
          //--------------------------------------------------------------------
          userHandler->HandleResponseWithHosts( status, response, hostList );
          lck.unlock();
          delete this;
          return;
        }

        //----------------------------------------------------------------------
        // We have to wait for retries!
        //----------------------------------------------------------------------
        resp.reset( response );
        hosts.reset( hostList );
        st.reset( status );
      }

      void UpdateCksum( size_t pgnb, uint32_t crcval )
      {
        if( resp )
        {
          XrdCl::PageInfo *pginf = 0;
          resp->Get( pginf );
          pginf->GetCksums()[pgnb] = crcval;
        }
      }

    private:

      XrdCl::FileStateHandler *stateHandler;
      XrdCl::ResponseHandler  *userHandler;

      uint64_t                 orgOffset;

      std::unique_ptr<XrdCl::AnyObject>    resp;
      std::unique_ptr<XrdCl::HostList>     hosts;
      std::unique_ptr<XrdCl::XRootDStatus> st;

      std::mutex mtx;
      bool       maincall;
      size_t     retrycnt;
      size_t     nbrepair;

  };

  //----------------------------------------------------------------------------
  // Helper callback for handling PgRead retries
  //----------------------------------------------------------------------------
  class PgReadRetryHandler : public XrdCl::ResponseHandler
  {
    public:

      PgReadRetryHandler( PgReadHandler *pgReadHandler, size_t pgnb ) : pgReadHandler( pgReadHandler ),
                                                                        pgnb( pgnb )
      {

      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *response,
                                    XrdCl::HostList     *hostList )
      {
        using namespace XrdCl;

        if( !status->IsOK() )
        {
          Log *log = DefaultEnv::GetLog();
          log->Info( FileMsg, "[0x%x@%s] Failed to recover page #%d.",
                      this, pgReadHandler->stateHandler->pFileUrl->GetURL().c_str(), pgnb );
          pgReadHandler->HandleResponseWithHosts( status, response, hostList );
          delete this;
          return;
        }

        XrdCl::PageInfo *pginf = 0;
        response->Get( pginf );
        if( pginf->GetLength() > (uint32_t)XrdSys::PageSize || pginf->GetCksums().size() != 1 )
        {
          Log *log = DefaultEnv::GetLog();
          log->Info( FileMsg, "[0x%x@%s] Failed to recover page #%d.",
                      this, pgReadHandler->stateHandler->pFileUrl->GetURL().c_str(), pgnb );
          // we retry a page at a time so the length cannot exceed 4KB
          DeleteArgs( status, response, hostList );
          pgReadHandler->HandleResponseWithHosts( new XRootDStatus( stError, errDataError ), 0, 0 );
          delete this;
          return;
        }

        uint32_t crcval = XrdOucCRC::Calc32C( pginf->GetBuffer(), pginf->GetLength() );
        if( crcval != pginf->GetCksums().front() )
        {
          Log *log = DefaultEnv::GetLog();
          log->Info( FileMsg, "[0x%x@%s] Failed to recover page #%d.",
                      this, pgReadHandler->stateHandler->pFileUrl->GetURL().c_str(), pgnb );
          DeleteArgs( status, response, hostList );
          pgReadHandler->HandleResponseWithHosts( new XRootDStatus( stError, errDataError ), 0, 0 );
          delete this;
          return;
        }

        Log *log = DefaultEnv::GetLog();
        log->Info( FileMsg, "[0x%x@%s] Successfully recovered page #%d.",
                    this, pgReadHandler->stateHandler->pFileUrl->GetURL().c_str(), pgnb );

        DeleteArgs( 0, response, hostList );
        pgReadHandler->UpdateCksum( pgnb, crcval );
        pgReadHandler->HandleResponseWithHosts( status, 0, 0 );
        delete this;
      }

    private:

      inline void DeleteArgs( XrdCl::XRootDStatus *status,
                              XrdCl::AnyObject    *response,
                              XrdCl::HostList     *hostList )
      {
        delete status;
        delete response;
        delete hostList;
      }

      PgReadHandler *pgReadHandler;
      size_t         pgnb;
  };

  //----------------------------------------------------------------------------
  // Handle PgRead substitution with ordinary Read
  //----------------------------------------------------------------------------
  class PgReadSubstitutionHandler : public XrdCl::ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      PgReadSubstitutionHandler( XrdCl::FileStateHandler *stateHandler,
                                 XrdCl::ResponseHandler  *userHandler ) : stateHandler( stateHandler ),
                                                                          userHandler( userHandler )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *rdresp,
                                    XrdCl::HostList     *hostList )
      {
        if( !status->IsOK() )
        {
          userHandler->HandleResponseWithHosts( status, rdresp, hostList );
          delete this;
          return;
        }

        using namespace XrdCl;

        ChunkInfo *chunk = 0;
        rdresp->Get( chunk );

        std::vector<uint32_t> cksums;
        if( stateHandler->pIsChannelEncrypted )
        {
          size_t nbpages = chunk->length / XrdSys::PageSize;
          if( chunk->length % XrdSys::PageSize )
            ++nbpages;
          cksums.reserve( nbpages );

          size_t  size = chunk->length;
          char   *buffer = reinterpret_cast<char*>( chunk->buffer );

          for( size_t pg = 0; pg < nbpages; ++pg )
          {
            size_t pgsize = XrdSys::PageSize;
            if( pgsize > size ) pgsize = size;
            uint32_t crcval = XrdOucCRC::Calc32C( buffer, pgsize );
            cksums.push_back( crcval );
            buffer += pgsize;
            size   -= pgsize;
          }
        }

        PageInfo *pages = new PageInfo( chunk->offset, chunk->length,
                                        chunk->buffer, std::move( cksums ) );
        delete rdresp;
        AnyObject *response = new AnyObject();
        response->Set( pages );
        userHandler->HandleResponseWithHosts( status, response, hostList );

        delete this;
      }

    private:

      XrdCl::FileStateHandler *stateHandler;
      XrdCl::ResponseHandler  *userHandler;
  };

  //----------------------------------------------------------------------------
  // Object that does things to the FileStateHandler when kXR_open returns
  // and then calls the user handler
  //----------------------------------------------------------------------------
  class OpenHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      OpenHandler( XrdCl::FileStateHandler *stateHandler,
                   XrdCl::ResponseHandler  *userHandler ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        using namespace XrdCl;

        //----------------------------------------------------------------------
        // Extract the statistics info
        //----------------------------------------------------------------------
        OpenInfo *openInfo = 0;
        if( status->IsOK() )
          response->Get( openInfo );
#ifdef WITH_XRDEC
        else
          //--------------------------------------------------------------------
          // Handle EC redirect
          //--------------------------------------------------------------------
          if( status->code == errRedirect )
          {
            std::string ecurl = status->GetErrorMessage();
            EcHandler *ecHandler = GetEcHandler( hostList->front().url, ecurl );
            if( ecHandler )
            {
              pStateHandler->pPlugin = ecHandler; // set the plugin for the File object
              ecHandler->Open( pStateHandler->pOpenFlags, pUserHandler, 0/*TODO figure out right value for the timeout*/ );
              return;
            }
          }
#endif
        //----------------------------------------------------------------------
        // Notify the state handler and the client and say bye bye
        //----------------------------------------------------------------------
        pStateHandler->OnOpen( status, openInfo, hostList );
        delete response;
        if( pUserHandler )
          pUserHandler->HandleResponseWithHosts( status, 0, hostList );
        else
        {
          delete status;
          delete hostList;
        }
        delete this;
      }

    private:
      XrdCl::FileStateHandler *pStateHandler;
      XrdCl::ResponseHandler  *pUserHandler;
  };

  //----------------------------------------------------------------------------
  // Object that does things to the FileStateHandler when kXR_close returns
  // and then calls the user handler
  //----------------------------------------------------------------------------
  class CloseHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      CloseHandler( XrdCl::FileStateHandler *stateHandler,
                    XrdCl::ResponseHandler  *userHandler,
                    XrdCl::Message          *message ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler ),
        pMessage( message )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CloseHandler()
      {
        delete pMessage;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        pStateHandler->OnClose( status );
        if( pUserHandler )
          pUserHandler->HandleResponseWithHosts( status, response, hostList );
        else
        {
          delete response;
          delete status;
          delete hostList;
        }

        delete this;
      }

    private:
      XrdCl::FileStateHandler *pStateHandler;
      XrdCl::ResponseHandler  *pUserHandler;
      XrdCl::Message          *pMessage;
  };

  //----------------------------------------------------------------------------
  // Stateful message handler
  //----------------------------------------------------------------------------
  class StatefulHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StatefulHandler( XrdCl::FileStateHandler        *stateHandler,
                       XrdCl::ResponseHandler         *userHandler,
                       XrdCl::Message                 *message,
                       const XrdCl::MessageSendParams &sendParams ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler ),
        pMessage( message ),
        pSendParams( sendParams )
      {
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~StatefulHandler()
      {
        delete pMessage;
        delete pSendParams.chunkList;
        delete pSendParams.kbuff;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        using namespace XrdCl;
        std::unique_ptr<AnyObject>       responsePtr( response );
        pSendParams.hostList = hostList;

        //----------------------------------------------------------------------
        // Houston we have a problem...
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          pStateHandler->OnStateError( status, pMessage, this, pSendParams );
          return;
        }

        //----------------------------------------------------------------------
        // We're clear
        //----------------------------------------------------------------------
        responsePtr.release();
        pStateHandler->OnStateResponse( status, pMessage, response, hostList );
        if( pUserHandler )
          pUserHandler->HandleResponseWithHosts( status, response, hostList );
        else
        {
          delete status,
          delete response;
          delete hostList;
        }
        delete this;
      }

      //------------------------------------------------------------------------
      //! Get the user handler
      //------------------------------------------------------------------------
      XrdCl::ResponseHandler *GetUserHandler()
      {
        return pUserHandler;
      }

    private:
      XrdCl::FileStateHandler  *pStateHandler;
      XrdCl::ResponseHandler   *pUserHandler;
      XrdCl::Message           *pMessage;
      XrdCl::MessageSendParams  pSendParams;
  };

  //----------------------------------------------------------------------------
  // Release-buffer Handler
  //----------------------------------------------------------------------------
  class ReleaseBufferHandler: public XrdCl::ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      ReleaseBufferHandler( XrdCl::Buffer &&buffer, XrdCl::ResponseHandler *handler ) :
        buffer( std::move( buffer ) ),
        handler( handler )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        handler->HandleResponseWithHosts( status, response, hostList );
      }

      //------------------------------------------------------------------------
      // Get the underlying buffer
      //------------------------------------------------------------------------
      XrdCl::Buffer& GetBuffer()
      {
        return buffer;
      }

    private:
      XrdCl::Buffer buffer;
      XrdCl::ResponseHandler *handler;
  };
}

namespace XrdCl
{
  //------------------------------------------------------------------------
  //! Holds a reference to a ResponceHandler
  //! and allows to safely delete it
  //------------------------------------------------------------------------
  class ResponseHandlerHolder : public ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ResponseHandlerHolder( ResponseHandler * handler ) : pHandler( handler ), pReferenceCounter( 1 ) {}
      //------------------------------------------------------------------------
      //! Destructor is private - use 'Destroy' in order to delete the object
      //! Always destroys the actual ResponseHandler and deletes itself only
      //! if this is the last reference
      //------------------------------------------------------------------------
      void Destroy()
      {
        XrdSysMutexHelper scopedLock( pMutex );
        // delete the actual handler
        if( pHandler )
        {
          delete pHandler;
          pHandler = 0;
        }
        // and than destroy myself if this is the last reference
        DestroyMyself( scopedLock );
      }
      //------------------------------------------------------------------------
      //! Increment reference counter
      //------------------------------------------------------------------------
      ResponseHandlerHolder* Self()
      {
        XrdSysMutexHelper scopedLock( pMutex );
        ++pReferenceCounter;
        return this;
      }
      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        // delegate the job to the actual handler
        if( pHandler )
        {
          pHandler->HandleResponseWithHosts( status, response, hostList );
          // after handling a response the handler destroys itself,
          // so we need to nullify the pointer
          pHandler = 0;
        }
        else
        {
          delete status;
          delete response;
          delete hostList;
        }
        // destroy the object if it is
        DestroyMyself( scopedLock );
      }

    private:
      //------------------------------------------------------------------------
      //! Deletes itself only if this is the last reference
      //------------------------------------------------------------------------
      void DestroyMyself( XrdSysMutexHelper &lck )
      {
        // decrement the reference counter
        --pReferenceCounter;
        // if the object is not used anymore delete it
        if( pReferenceCounter == 0)
        {
          lck.UnLock();
          delete this;
        }
      }
      //------------------------------------------------------------------------
      //! Private Destructor (use 'Destroy' method)
      //------------------------------------------------------------------------
      ~ResponseHandlerHolder() {}
      //------------------------------------------------------------------------
      // The actual handler
      //------------------------------------------------------------------------
      ResponseHandler* pHandler;
      //------------------------------------------------------------------------
      // Reference counter
      //------------------------------------------------------------------------
      size_t pReferenceCounter;
      //------------------------------------------------------------------------
      // and respective mutex
      //------------------------------------------------------------------------
      mutable XrdSysRecMutex pMutex;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  FileStateHandler::FileStateHandler( FilePlugIn *& plugin ):
    pFileState( Closed ),
    pStatInfo( 0 ),
    pFileUrl( 0 ),
    pDataServer( 0 ),
    pLoadBalancer( 0 ),
    pStateRedirect( 0 ),
    pWrtRecoveryRedir( 0 ),
    pFileHandle( 0 ),
    pOpenMode( 0 ),
    pOpenFlags( 0 ),
    pSessionId( 0 ),
    pDoRecoverRead( true ),
    pDoRecoverWrite( true ),
    pFollowRedirects( true ),
    pUseVirtRedirector( true ),
    pIsChannelEncrypted( false ),
    pAllowBundledClose( false ),
    pReOpenHandler( 0 ),
    pPlugin( plugin )
  {
    pFileHandle = new uint8_t[4];
    ResetMonitoringVars();
    DefaultEnv::GetForkHandler()->RegisterFileObject( this );
    DefaultEnv::GetFileTimer()->RegisterFileObject( this );
    pLFileHandler = new LocalFileHandler();
  }

  //------------------------------------------------------------------------
  //! Constructor
  //!
  //! @param useVirtRedirector if true Metalink files will be treated
  //!                          as a VirtualRedirectors
  //------------------------------------------------------------------------
  FileStateHandler::FileStateHandler( bool useVirtRedirector, FilePlugIn *& plugin ):
    pFileState( Closed ),
    pStatInfo( 0 ),
    pFileUrl( 0 ),
    pDataServer( 0 ),
    pLoadBalancer( 0 ),
    pStateRedirect( 0 ),
    pWrtRecoveryRedir( 0 ),
    pFileHandle( 0 ),
    pOpenMode( 0 ),
    pOpenFlags( 0 ),
    pSessionId( 0 ),
    pDoRecoverRead( true ),
    pDoRecoverWrite( true ),
    pFollowRedirects( true ),
    pUseVirtRedirector( useVirtRedirector ),
    pAllowBundledClose( false ),
    pReOpenHandler( 0 ),
    pPlugin( plugin )
  {
    pFileHandle = new uint8_t[4];
    ResetMonitoringVars();
    DefaultEnv::GetForkHandler()->RegisterFileObject( this );
    DefaultEnv::GetFileTimer()->RegisterFileObject( this );
    pLFileHandler = new LocalFileHandler();
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  FileStateHandler::~FileStateHandler()
  {
    //--------------------------------------------------------------------------
    // This, in principle, should never ever happen. Except for the case
    // when we're interfaced with ROOT that may call this desctructor from
    // its garbage collector, from its __cxa_finalize, ie. after the XrdCl lib
    // has been finalized by the linker. So, if we don't have the log object
    // at this point we just give up the hope.
    //--------------------------------------------------------------------------
    if( DefaultEnv::GetLog() && pSessionId && !pDataServer->IsLocalFile() ) // if the file object was bound to a physical connection
      DefaultEnv::GetPostMaster()->DecFileInstCnt( *pDataServer );

    if( pReOpenHandler )
      pReOpenHandler->Destroy();

    if( DefaultEnv::GetFileTimer() )
      DefaultEnv::GetFileTimer()->UnRegisterFileObject( this );

    if( DefaultEnv::GetForkHandler() )
      DefaultEnv::GetForkHandler()->UnRegisterFileObject( this );

    if( pFileState != Closed && DefaultEnv::GetLog() )
    {
      XRootDStatus st;
      MonitorClose( &st );
      ResetMonitoringVars();
    }

    // check if the logger is still there, this is only for root, as root might
    // have unload us already so in this case we don't want to do anything
    if( DefaultEnv::GetLog() && pUseVirtRedirector && pFileUrl && pFileUrl->IsMetalink() )
    {
      RedirectorRegistry& registry = RedirectorRegistry::Instance();
      registry.Release( *pFileUrl );
    }

    delete pStatInfo;
    delete pFileUrl;
    delete pDataServer;
    delete pLoadBalancer;
    delete [] pFileHandle;
    delete pLFileHandler;
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Open( const std::string &url,
                                       uint16_t           flags,
                                       uint16_t           mode,
                                       ResponseHandler   *handler,
                                       uint16_t           timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Check if we can proceed
    //--------------------------------------------------------------------------
    if( pFileState == Error )
      return pStatus;

    if( pFileState == OpenInProgress )
      return XRootDStatus( stError, errInProgress );

    if( pFileState == CloseInProgress || pFileState == Opened ||
        pFileState == Recovering )
      return XRootDStatus( stError, errInvalidOp );

    pFileState = OpenInProgress;

    //--------------------------------------------------------------------------
    // Check if the parameters are valid
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();

    if (pFileUrl)
    {
      if( pUseVirtRedirector && pFileUrl->IsMetalink() )
      {
        RedirectorRegistry& registry = RedirectorRegistry::Instance();
        registry.Release( *pFileUrl );
      }
      delete pFileUrl;
      pFileUrl = 0;
    }

    pFileUrl = new URL( url );

    //--------------------------------------------------------------------------
    // Add unique uuid to each open request so replays due to error/timeout
    // recovery can be correctly handled.
    //--------------------------------------------------------------------------
    URL::ParamsMap cgi = pFileUrl->GetParams();
    uuid_t uuid;
    char requuid[37]= {0};
    uuid_generate( uuid );
    uuid_unparse( uuid, requuid );
    cgi["xrdcl.requuid"] = requuid;
    pFileUrl->SetParams( cgi );

    if( !pFileUrl->IsValid() )
    {
      log->Error( FileMsg, "[0x%x@%s] Trying to open invalid url: %s",
                  this, pFileUrl->GetPath().c_str(), url.c_str() );
      pStatus    = XRootDStatus( stError, errInvalidArgs );
      pFileState = Closed;
      return pStatus;
    }

    //--------------------------------------------------------------------------
    // Check if the recovery procedures should be enabled
    //--------------------------------------------------------------------------
    const URL::ParamsMap &urlParams = pFileUrl->GetParams();
    URL::ParamsMap::const_iterator it;
    it = urlParams.find( "xrdcl.recover-reads" );
    if( (it != urlParams.end() && it->second == "false") ||
        !pDoRecoverRead )
    {
      pDoRecoverRead = false;
      log->Debug( FileMsg, "[0x%x@%s] Read recovery procedures are disabled",
                  this, pFileUrl->GetURL().c_str() );
    }

    it = urlParams.find( "xrdcl.recover-writes" );
    if( (it != urlParams.end() && it->second == "false") ||
        !pDoRecoverWrite )
    {
      pDoRecoverWrite = false;
      log->Debug( FileMsg, "[0x%x@%s] Write recovery procedures are disabled",
                  this, pFileUrl->GetURL().c_str() );
    }

    //--------------------------------------------------------------------------
    // Open the file
    //--------------------------------------------------------------------------
    log->Debug( FileMsg, "[0x%x@%s] Sending an open command", this,
                pFileUrl->GetURL().c_str() );

    pOpenMode  = mode;
    pOpenFlags = flags;
    OpenHandler *openHandler = new OpenHandler( this, handler );

    Message           *msg;
    ClientOpenRequest *req;
    std::string        path = pFileUrl->GetPathWithFilteredParams();
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_open;
    req->mode      = mode;
    req->options   = flags | kXR_async | kXR_retstat;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    XRootDTransport::SetDescription( msg );
    MessageSendParams params; params.timeout = timeout;
    params.followRedirects = pFollowRedirects;
    MessageUtils::ProcessSendParams( params );

    XRootDStatus st = IssueRequest( *pFileUrl, msg, openHandler, params );

    if( !st.IsOK() )
    {
      delete openHandler;
      pStatus    = st;
      pFileState = Closed;
      return st;
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Close the file object
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Close( ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Check if we can proceed
    //--------------------------------------------------------------------------
    if( pFileState == Error )
      return pStatus;

    if( pFileState == CloseInProgress )
      return XRootDStatus( stError, errInProgress );

    if( pFileState == Closed )
      return XRootDStatus( stOK, suAlreadyDone );

    if( pFileState == OpenInProgress || pFileState == Recovering )
      return XRootDStatus( stError, errInvalidOp );

    if( !pAllowBundledClose && !pInTheFly.empty() )
      return XRootDStatus( stError, errInvalidOp );

    pFileState = CloseInProgress;

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a close command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Close the file
    //--------------------------------------------------------------------------
    Message            *msg;
    ClientCloseRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_close;
    memcpy( req->fhandle, pFileHandle, 4 );

    XRootDTransport::SetDescription( msg );
    msg->SetSessionId( pSessionId );
    CloseHandler *closeHandler = new CloseHandler( this, handler, msg );
    MessageSendParams params;
    params.timeout = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDStatus st = IssueRequest( *pDataServer, msg, closeHandler, params );

    if( !st.IsOK() )
    {
      // an invalid-session error means the connection to the server has been
      // closed, which in turn means that the server closed the file already
      if( st.code == errInvalidSession  || st.code == errSocketDisconnected ||
          st.code == errConnectionError || st.code == errSocketOptError     ||
          st.code == errPollerError     || st.code == errSocketError           )
      {
        pFileState = Closed;
        ResponseJob *job = new ResponseJob( closeHandler, new XRootDStatus(),
                                            nullptr, nullptr );
        DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
        return XRootDStatus();
      }

      delete closeHandler;
      pStatus    = st;
      pFileState = Error;
      return st;
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Stat the file
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Stat( bool             force,
                                       ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Return the cached info
    //--------------------------------------------------------------------------
    if( !force )
    {
      AnyObject *obj = new AnyObject();
      obj->Set( new StatInfo( *pStatInfo ) );
      handler->HandleResponseWithHosts( new XRootDStatus(), obj,
                                        new HostList() );
      return XRootDStatus();
    }

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a stat command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Issue a new stat request
    // stating a file handle doesn't work (fixed in 3.2.0) so we need to
    // stat the pat
    //--------------------------------------------------------------------------
    Message           *msg;
    ClientStatRequest *req;
    std::string        path = pFileUrl->GetPath();
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_stat;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Read( uint64_t         offset,
                                       uint32_t         size,
                                       void            *buffer,
                                       ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a read command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message           *msg;
    ClientReadRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_read;
    req->offset     = offset;
    req->rlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( offset, size, buffer ) );

    XRootDTransport::SetDescription( msg );
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );
    StatefulHandler  *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  // Read data pages at a given offset
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::PgRead( uint64_t         offset,
                                         uint32_t         size,
                                         void            *buffer,
                                         ResponseHandler *handler,
                                         uint16_t         timeout )
  {
    int issupported = true;
    AnyObject obj;
    XRootDStatus st = DefaultEnv::GetPostMaster()->QueryTransport( *pDataServer, XRootDQuery::ServerFlags, obj );
    if( st.IsOK() )
    {
      int *ptr = 0;
      obj.Get( ptr );
      issupported = ( *ptr & kXR_suppgrw );
      delete ptr;
    }

    if( !issupported )
    {
      DefaultEnv::GetLog()->Debug( FileMsg, "[0x%x@%s] PgRead not supported; substituting with Read.",
                                  this, pFileUrl->GetURL().c_str() );
      ResponseHandler *substitHandler = new PgReadSubstitutionHandler( this, handler );
      st = Read( offset, size, buffer, substitHandler, timeout );
      if( !st.IsOK() ) delete substitHandler;
      return st;
    }

    ResponseHandler* pgHandler = new PgReadHandler( this, handler, offset );
    st = PgReadImpl( offset, size, buffer, PgReadFlags::None, pgHandler, timeout );
    if( !st.IsOK() ) delete pgHandler;
    return st;
  }

  XRootDStatus FileStateHandler::PgReadRetry( uint64_t        offset,
                                              uint32_t        size,
                                              size_t          pgnb,
                                              void           *buffer,
                                              PgReadHandler  *handler,
                                              uint16_t        timeout )
  {
    if( size > (uint32_t)XrdSys::PageSize )
      return XRootDStatus( stError, errInvalidArgs, EINVAL,
                          "PgRead retry size exceeded 4KB." );

    ResponseHandler *retryHandler = new PgReadRetryHandler( handler, pgnb );
    XRootDStatus st = PgReadImpl( offset, size, buffer, PgReadFlags::Retry, retryHandler, timeout );
    if( !st.IsOK() ) delete retryHandler;
    return st;
  }

  XRootDStatus FileStateHandler::PgReadImpl( uint64_t         offset,
                                             uint32_t         size,
                                             void            *buffer,
                                             uint16_t         flags,
                                             ResponseHandler *handler,
                                             uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a pgread command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message             *msg;
    ClientPgReadRequest *req;
    MessageUtils::CreateRequest( msg, req, sizeof( ClientPgReadReqArgs ) );

    req->requestid  = kXR_pgread;
    req->offset     = offset;
    req->rlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    //--------------------------------------------------------------------------
    // Now adjust the message size so it can hold PgRead arguments
    //--------------------------------------------------------------------------
    req->dlen = sizeof( ClientPgReadReqArgs );
    void *newBuf = msg->GetBuffer( sizeof( ClientPgReadRequest ) );
    memset( newBuf, 0, sizeof( ClientPgReadReqArgs ) );
    ClientPgReadReqArgs *args = reinterpret_cast<ClientPgReadReqArgs*>(
        msg->GetBuffer( sizeof( ClientPgReadRequest ) ) );
    args->reqflags = flags;

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( offset, size, buffer ) );

    XRootDTransport::SetDescription( msg );
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Write( uint64_t         offset,
                                        uint32_t         size,
                                        const void      *buffer,
                                        ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a write command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientWriteRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_write;
    req->offset     = offset;
    req->dlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( 0, size, (char*)buffer ) );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Write( uint64_t          offset,
                                        Buffer          &&buffer,
                                        ResponseHandler  *handler,
                                        uint16_t          timeout )
  {
    //--------------------------------------------------------------------------
    // If the memory is not page (4KB) aligned we cannot use the kernel buffer
    // so fall back to normal write
    //--------------------------------------------------------------------------
    if( !XrdSys::KernelBuffer::IsPageAligned( buffer.GetBuffer() ) || pIsChannelEncrypted )
    {
      Log *log = DefaultEnv::GetLog();
      log->Info( FileMsg, "[0x%x@%s] Buffer is not page aligned (4KB), cannot "
                 "convert it to kernel space buffer.", this, pFileUrl->GetURL().c_str(),
                 *((uint32_t*)pFileHandle) );

      void     *buff = buffer.GetBuffer();
      uint32_t  size = buffer.GetSize();
      ReleaseBufferHandler *wrtHandler =
          new ReleaseBufferHandler( std::move( buffer ), handler );
      XRootDStatus st = Write( offset, size, buff, wrtHandler, timeout );
      if( !st.IsOK() )
      {
        buffer = std::move( wrtHandler->GetBuffer() );
        delete wrtHandler;
      }
      return st;
    }

    //--------------------------------------------------------------------------
    // Transfer the data from user space to kernel space
    //--------------------------------------------------------------------------
    uint32_t  length = buffer.GetSize();
    char     *ubuff  = buffer.Release();

    std::unique_ptr<XrdSys::KernelBuffer> kbuff( new XrdSys::KernelBuffer() );
    ssize_t ret = XrdSys::Move( ubuff, *kbuff, length );
    if( ret < 0 )
      return XRootDStatus( stError, errInternal, XProtocol::mapError( errno ) );

    //--------------------------------------------------------------------------
    // Now create a write request and enqueue it
    //--------------------------------------------------------------------------
    return WriteKernelBuffer( offset, ret, std::move( kbuff ), handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Write a data from a given file descriptor at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Write( uint64_t            offset,
                                        uint32_t            size,
                                        Optional<uint64_t>  fdoff,
                                        int                 fd,
                                        ResponseHandler    *handler,
                                        uint16_t            timeout )
  {
    //--------------------------------------------------------------------------
    // Read the data from the file descriptor into a kernel buffer
    //--------------------------------------------------------------------------
    std::unique_ptr<XrdSys::KernelBuffer> kbuff( new XrdSys::KernelBuffer() );
    ssize_t ret = fdoff ? XrdSys::Read( fd, *kbuff, size, *fdoff ) :
                          XrdSys::Read( fd, *kbuff, size );
    if( ret < 0 )
      return XRootDStatus( stError, errInternal, XProtocol::mapError( errno ) );

    //--------------------------------------------------------------------------
    // Now create a write request and enqueue it
    //--------------------------------------------------------------------------
    return WriteKernelBuffer( offset, ret, std::move( kbuff ), handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Write number of pages at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::PgWrite( uint64_t               offset,
                                          uint32_t               size,
                                          const void            *buffer,
                                          std::vector<uint32_t> &cksums,
                                          ResponseHandler       *handler,
                                          uint16_t               timeout )
  {
    //--------------------------------------------------------------------------
    // Resolve timeout value
    //--------------------------------------------------------------------------
    if( timeout == 0 )
    {
      int val = DefaultRequestTimeout;
      XrdCl::DefaultEnv::GetEnv()->GetInt( "RequestTimeout", val );
      timeout = val;
    }

    //--------------------------------------------------------------------------
    // Validate the digest vector size
    //--------------------------------------------------------------------------
    if( cksums.empty() )
    {
      const char *data = static_cast<const char*>( buffer );
      XrdOucPgrwUtils::csCalc( data, offset, size, cksums );
    }
    else
    {
      size_t crc32cCnt = XrdOucPgrwUtils::csNum( offset, size );
      if( crc32cCnt != cksums.size() )
        return XRootDStatus( stError, errInvalidArgs, 0, "Wrong number of crc32c digests." );
    }

    //--------------------------------------------------------------------------
    // Create a context for PgWrite operation
    //--------------------------------------------------------------------------
    struct pgwrt_t
    {
      pgwrt_t( ResponseHandler *h ) : handler( h ), status( nullptr )
      {
      }

      ~pgwrt_t()
      {
        if( handler )
        {
          // if all retries were successful no error status was set
          if( !status ) status = new XRootDStatus();
          handler->HandleResponse( status, nullptr );
        }
      }

      static size_t GetPgNb( uint64_t pgoff, uint64_t offset, uint32_t fstpglen )
      {
        if( pgoff == offset ) return 0; // we need this if statement because we operate on unsigned integers
        return ( pgoff - ( offset + fstpglen ) ) / XrdSys::PageSize + 1;
      }

      inline void SetStatus( XRootDStatus* s )
      {
        if( !status ) status = s;
        else delete s;
      }

      ResponseHandler *handler;
      XRootDStatus    *status;
    };
    auto pgwrt = std::make_shared<pgwrt_t>( handler );

    int fLen, lLen;
    XrdOucPgrwUtils::csNum( offset, size, fLen, lLen );
    uint32_t fstpglen = fLen;

    time_t start = ::time( nullptr );
    auto h = ResponseHandler::Wrap( [=]( auto* s, auto* r ) mutable
        {
          std::unique_ptr<AnyObject> scoped( r );
          // if the request failed simply pass the status to the
          // user handler
          if( !s->IsOK() )
          {
            pgwrt->SetStatus( s );
            return; // pgwrt destructor will call the handler
          }
          // also if the request was sucessful and there were no
          // corrupted pages pass the status to the user handler
          RetryInfo *inf = nullptr;
          r->Get( inf );
          if( !inf->NeedRetry() )
          {
            pgwrt->SetStatus( s );
            return; // pgwrt destructor will call the handler
          }
          delete s;
          // first adjust the timeout value
          uint16_t elapsed = ::time( nullptr ) - start;
          if( elapsed >= timeout )
          {
            pgwrt->SetStatus( new XRootDStatus( stError, errOperationExpired ) );
            return; // pgwrt destructor will call the handler
          }
          else timeout -= elapsed;
          // retransmit the corrupted pages
          for( size_t i = 0; i < inf->Size(); ++i )
          {
            auto tpl = inf->At( i );
            uint64_t    pgoff = std::get<0>( tpl );
            uint32_t    pglen = std::get<1>( tpl );
            const void *pgbuf = static_cast<const char*>( buffer ) + ( pgoff - offset );
            uint32_t pgdigest = cksums[pgwrt_t::GetPgNb( pgoff, offset, fstpglen )];
            auto h = ResponseHandler::Wrap( [=]( auto *s, auto *r ) mutable
                {
                  std::unique_ptr<AnyObject> scoped( r );
                  // if we failed simply set the status
                  if( !s->IsOK() )
                  {
                    pgwrt->SetStatus( s );
                    return; // the destructor will call the handler
                  }
                  delete s;
                  // otherwise check if the data were not corrupted again
                  RetryInfo *inf = nullptr;
                  r->Get( inf );
                  if( inf->NeedRetry() ) // so we failed in the end
                  {
                    DefaultEnv::GetLog()->Warning( FileMsg, "[0x%x@%s] Failed retransmitting corrupted "
                                                   "page: pgoff=%llu, pglen=%du, pgdigest=%du", this,
                                                   pFileUrl->GetURL().c_str(), pgoff, pglen, pgdigest );
                    pgwrt->SetStatus( new XRootDStatus( stError, errDataError, 0,
                                      "Failed to retransmit corrupted page" ) );
                  }
                  else
                    DefaultEnv::GetLog()->Info( FileMsg, "[0x%x@%s] Succesfuly retransmitted corrupted "
                                                "page: pgoff=%llu, pglen=%du, pgdigest=%du", this,
                                                pFileUrl->GetURL().c_str(), pgoff, pglen, pgdigest );
                } );
            auto st = this->PgWriteRetry( pgoff, pglen, pgbuf, pgdigest, h, timeout );
            if( !st.IsOK() ) pgwrt->SetStatus( new XRootDStatus( st ) );
            DefaultEnv::GetLog()->Info( FileMsg, "[0x%x@%s] Retransmitting corrupted page: "
                                        "pgoff=%llu, pglen=%du, pgdigest=%du", this,
                                        pFileUrl->GetURL().c_str(), pgoff, pglen, pgdigest );
          }
        } );

    auto st = PgWriteImpl( offset, size, buffer, cksums, 0, h, timeout );
    if( !st.IsOK() )
    {
      pgwrt->handler = nullptr;
      delete h;
    }
    return st;
  }

  //------------------------------------------------------------------------
  // Write number of pages at a given offset - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::PgWriteRetry( uint64_t             offset,
                                               uint32_t               size,
                                               const void            *buffer,
                                               uint32_t               digest,
                                               ResponseHandler       *handler,
                                               uint16_t               timeout )
  {
    std::vector<uint32_t> cksums{ digest };
    return PgWriteImpl( offset, size, buffer, cksums, PgReadFlags::Retry, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Write number of pages at a given offset - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::PgWriteImpl( uint64_t             offset,
                                              uint32_t               size,
                                              const void            *buffer,
                                              std::vector<uint32_t> &cksums,
                                              kXR_char               flags,
                                              ResponseHandler       *handler,
                                              uint16_t               timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a pgwrite command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Create the message
    //--------------------------------------------------------------------------
    Message              *msg;
    ClientPgWriteRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_pgwrite;
    req->offset     = offset;
    req->dlen       = size + cksums.size() * sizeof( uint32_t );
    req->reqflags   = flags;
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( offset, size, (char*)buffer ) );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    params.crc32cDigests.swap( cksums );

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Sync( ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a sync command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message           *msg;
    ClientSyncRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_sync;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Truncate the file to a particular size - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Truncate( uint64_t         size,
                                           ResponseHandler *handler,
                                           uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a truncate command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message               *msg;
    ClientTruncateRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_truncate;
    memcpy( req->fhandle, pFileHandle, 4 );
    req->offset = size;

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Read scattered data chunks in one operation - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::VectorRead( const ChunkList &chunks,
                                             void            *buffer,
                                             ResponseHandler *handler,
                                             uint16_t         timeout )
  {
    //--------------------------------------------------------------------------
    // Sanity check
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a vector read command for handle "
                "0x%x to %s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Build the message
    //--------------------------------------------------------------------------
    Message            *msg;
    ClientReadVRequest *req;
    MessageUtils::CreateRequest( msg, req, sizeof(readahead_list)*chunks.size() );

    req->requestid = kXR_readv;
    req->dlen      = sizeof(readahead_list)*chunks.size();

    ChunkList *list   = new ChunkList();
    char      *cursor = (char*)buffer;

    //--------------------------------------------------------------------------
    // Copy the chunk info
    //--------------------------------------------------------------------------
    readahead_list *dataChunk = (readahead_list*)msg->GetBuffer( 24 );
    for( size_t i = 0; i < chunks.size(); ++i )
    {
      dataChunk[i].rlen   = chunks[i].length;
      dataChunk[i].offset = chunks[i].offset;
      memcpy( dataChunk[i].fhandle, pFileHandle, 4 );

      void *chunkBuffer;
      if( cursor )
      {
        chunkBuffer  = cursor;
        cursor      += chunks[i].length;
      }
      else
        chunkBuffer = chunks[i].buffer;

      list->push_back( ChunkInfo( chunks[i].offset,
                                  chunks[i].length,
                                  chunkBuffer ) );
    }

    //--------------------------------------------------------------------------
    // Send the message
    //--------------------------------------------------------------------------
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  // Write scattered data chunks in one operation - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::VectorWrite( const ChunkList &chunks,
                                              ResponseHandler *handler,
                                              uint16_t         timeout )
  {
    //--------------------------------------------------------------------------
    // Sanity check
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a vector write command for handle "
                "0x%x to %s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Determine the size of the payload
    //--------------------------------------------------------------------------

    // the size of write vector
    uint32_t payloadSize = sizeof(XrdProto::write_list) * chunks.size();

    //--------------------------------------------------------------------------
    // Build the message
    //--------------------------------------------------------------------------
    Message             *msg;
    ClientWriteVRequest *req;
    MessageUtils::CreateRequest( msg, req, payloadSize );

    req->requestid = kXR_writev;
    req->dlen      = sizeof(XrdProto::write_list) * chunks.size();

    ChunkList *list   = new ChunkList();

    //--------------------------------------------------------------------------
    // Copy the chunk info
    //--------------------------------------------------------------------------
    XrdProto::write_list *writeList =
        reinterpret_cast<XrdProto::write_list*>( msg->GetBuffer( 24 ) );



    for( size_t i = 0; i < chunks.size(); ++i )
    {
      writeList[i].wlen   = chunks[i].length;
      writeList[i].offset = chunks[i].offset;
      memcpy( writeList[i].fhandle, pFileHandle, 4 );

      list->push_back( ChunkInfo( chunks[i].offset,
                                  chunks[i].length,
                                  chunks[i].buffer ) );
    }

    //--------------------------------------------------------------------------
    // Send the message
    //--------------------------------------------------------------------------
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  // Write scattered buffers in one operation - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::WriteV( uint64_t            offset,
                                         const struct iovec *iov,
                                         int                 iovcnt,
                                         ResponseHandler    *handler,
                                         uint16_t            timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a write command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientWriteRequest *req;
    MessageUtils::CreateRequest( msg, req );

    ChunkList *list   = new ChunkList();

    uint32_t size = 0;
    for( int i = 0; i < iovcnt; ++i )
    {
      if( iov[i].iov_len == 0 ) continue;
      size += iov[i].iov_len;
      list->push_back( ChunkInfo( 0, iov[i].iov_len,
                       (char*)iov[i].iov_base ) );
    }

    req->requestid  = kXR_write;
    req->offset     = offset;
    req->dlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  // Read data into scattered buffers in one operation - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::ReadV( uint64_t         offset,
                                        struct iovec    *iov,
                                        int              iovcnt,
                                        ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a read command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message           *msg;
    ClientReadRequest *req;
    MessageUtils::CreateRequest( msg, req );

    // calculate the total read size
    size_t size = std::accumulate( iov, iov + iovcnt, 0, []( size_t acc, iovec &rhs )
                                                         {
                                                           return acc + rhs.iov_len;
                                                         } );
    req->requestid  = kXR_read;
    req->offset     = offset;
    req->rlen       = size;
    msg->SetVirtReqID( kXR_virtReadv );
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list = new ChunkList();
    list->reserve( iovcnt );
    uint64_t choff = offset;
    for( int i = 0; i < iovcnt; ++i )
    {
      list->emplace_back( choff, iov[i].iov_len, iov[i].iov_base );
      choff += iov[i].iov_len;
    }

    XRootDTransport::SetDescription( msg );
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Performs a custom operation on an open file, server implementation
  // dependent - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Fcntl( const Buffer    &arg,
                                        ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a fcntl command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientQueryRequest *req;
    MessageUtils::CreateRequest( msg, req, arg.GetSize() );

    req->requestid = kXR_query;
    req->infotype  = kXR_Qopaqug;
    req->dlen      = arg.GetSize();
    memcpy( req->fhandle, pFileHandle, 4 );
    msg->Append( arg.GetBuffer(), arg.GetSize(), 24 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Get access token to a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Visa( ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a visa command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientQueryRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_query;
    req->infotype  = kXR_Qvisa;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  // Set extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::SetXAttr( const std::vector<xattr_t> &attrs,
                                           ResponseHandler            *handler,
                                           uint16_t                    timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a fattr set command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Issue a new fattr get request
    //--------------------------------------------------------------------------
    return XAttrOperationImpl( kXR_fattrSet, 0, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Get extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::GetXAttr( const std::vector<std::string> &attrs,
                                           ResponseHandler                *handler,
                                           uint16_t                        timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a fattr get command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Issue a new fattr get request
    //--------------------------------------------------------------------------
    return XAttrOperationImpl( kXR_fattrGet, 0, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Delete extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::DelXAttr( const std::vector<std::string> &attrs,
                                           ResponseHandler                *handler,
                                           uint16_t                        timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a fattr del command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Issue a new fattr del request
    //--------------------------------------------------------------------------
    return XAttrOperationImpl( kXR_fattrDel, 0, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // List extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::ListXAttr( ResponseHandler  *handler,
                                            uint16_t          timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a fattr list command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Issue a new fattr get request
    //--------------------------------------------------------------------------
    static const std::vector<std::string> nothing;
    return XAttrOperationImpl( kXR_fattrList, ClientFattrRequest::aData,
                               nothing, handler, timeout );
  }

  //------------------------------------------------------------------------
  //! Create a checkpoint
  //!
  //! @param handler : handler to be notified when the response arrives,
  //!                  the response parameter will hold a std::vector of
  //!                  XAttr objects
  //! @param timeout : timeout value, if 0 the environment default will
  //!                  be used
  //!
  //! @return        : status of the operation
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Checkpoint( kXR_char          code,
                                             ResponseHandler  *handler,
                                             uint16_t          timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a checkpoint command for "
                "handle 0x%x to %s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message               *msg;
    ClientChkPointRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_chkpoint;
    req->opcode     = code;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  //! Checkpointed write - async
  //!
  //! @param offset  offset from the beginning of the file
  //! @param size    number of bytes to be written
  //! @param buffer  a pointer to the buffer holding the data to be written
  //! @param handler handler to be notified when the response arrives
  //! @param timeout timeout value, if 0 the environment default will be
  //!                used
  //! @return        status of the operation
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::ChkptWrt( uint64_t         offset,
                                           uint32_t         size,
                                           const void      *buffer,
                                           ResponseHandler *handler,
                                           uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a write command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message               *msg;
    ClientChkPointRequest *req;
    MessageUtils::CreateRequest( msg, req, sizeof( ClientWriteRequest ) );

    req->requestid = kXR_chkpoint;
    req->opcode    = kXR_ckpXeq;
    req->dlen      = 24; // as specified in the protocol specification
    memcpy( req->fhandle, pFileHandle, 4 );

    ClientWriteRequest *wrtreq = (ClientWriteRequest*)msg->GetBuffer( sizeof(ClientChkPointRequest) );
    wrtreq->requestid = kXR_write;
    wrtreq->offset    = offset;
    wrtreq->dlen      = size;
    memcpy( wrtreq->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( 0, size, (char*)buffer ) );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //------------------------------------------------------------------------
  //! Write scattered buffers in one operation - async
  //!
  //! @param offset    offset from the beginning of the file
  //! @param iov       list of the buffers to be written
  //! @param iovcnt    number of buffers
  //! @param handler   handler to be notified when the response arrives
  //! @param timeout   timeout value, if 0 then the environment default
  //!                  will be used
  //! @return          status of the operation
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::ChkptWrtV( uint64_t            offset,
                                            const struct iovec *iov,
                                            int                 iovcnt,
                                            ResponseHandler    *handler,
                                            uint16_t            timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Error ) return pStatus;

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a write command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message               *msg;
    ClientChkPointRequest *req;
    MessageUtils::CreateRequest( msg, req, sizeof( ClientWriteRequest ) );

    req->requestid = kXR_chkpoint;
    req->opcode    = kXR_ckpXeq;
    req->dlen      = 24; // as specified in the protocol specification
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    uint32_t size = 0;
    for( int i = 0; i < iovcnt; ++i )
    {
      if( iov[i].iov_len == 0 ) continue;
      size += iov[i].iov_len;
      list->push_back( ChunkInfo( 0, iov[i].iov_len,
                       (char*)iov[i].iov_base ) );
    }

    ClientWriteRequest *wrtreq = (ClientWriteRequest*)msg->GetBuffer( sizeof(ClientChkPointRequest) );
    wrtreq->requestid = kXR_write;
    wrtreq->offset    = offset;
    wrtreq->dlen      = size;
    memcpy( wrtreq->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Check if the file is open
  //----------------------------------------------------------------------------
  bool FileStateHandler::IsOpen() const
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Opened || pFileState == Recovering )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Set file property
  //----------------------------------------------------------------------------
  bool FileStateHandler::SetProperty( const std::string &name,
                                      const std::string &value )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( name == "ReadRecovery" )
    {
      if( value == "true" ) pDoRecoverRead = true;
      else pDoRecoverRead = false;
      return true;
    }
    else if( name == "WriteRecovery" )
    {
      if( value == "true" ) pDoRecoverWrite = true;
      else pDoRecoverWrite = false;
      return true;
    }
    else if( name == "FollowRedirects" )
    {
      if( value == "true" ) pFollowRedirects = true;
      else pFollowRedirects = false;
      return true;
    }
    else if( name == "BundledClose" )
    {
      if( value == "true" ) pAllowBundledClose = true;
      else pAllowBundledClose = false;
      return true;
    }
    return false;
  }

  //----------------------------------------------------------------------------
  // Get file property
  //----------------------------------------------------------------------------
  bool FileStateHandler::GetProperty( const std::string &name,
                                      std::string &value ) const
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( name == "ReadRecovery" )
    {
      if( pDoRecoverRead ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "WriteRecovery" )
    {
      if( pDoRecoverWrite ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "FollowRedirects" )
    {
      if( pFollowRedirects ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "DataServer" && pDataServer )
      { value = pDataServer->GetHostId(); return true; }
    else if( name == "LastURL" && pDataServer )
      { value =  pDataServer->GetURL(); return true; }
    else if( name == "WrtRecoveryRedir" && pWrtRecoveryRedir )
      { value = pWrtRecoveryRedir->GetHostId(); return true; }
    value = "";
    return false;
  }

  //----------------------------------------------------------------------------
  // Process the results of the opening operation
  //----------------------------------------------------------------------------
  void FileStateHandler::OnOpen( const XRootDStatus *status,
                                 const OpenInfo     *openInfo,
                                 const HostList     *hostList )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Assign the data server and the load balancer
    //--------------------------------------------------------------------------
    std::string lastServer = pFileUrl->GetHostId();
    if( hostList )
    {
      delete pDataServer;
      delete pLoadBalancer;
      pLoadBalancer = 0;
      delete pWrtRecoveryRedir;
      pWrtRecoveryRedir = 0;

      pDataServer = new URL( hostList->back().url );
      pDataServer->SetParams( pFileUrl->GetParams() );
      if( !( pUseVirtRedirector && pFileUrl->IsMetalink() ) ) pDataServer->SetPath( pFileUrl->GetPath() );
      lastServer = pDataServer->GetHostId();
      HostList::const_iterator itC;
      URL::ParamsMap params = pDataServer->GetParams();
      for( itC = hostList->begin(); itC != hostList->end(); ++itC )
      {
        MessageUtils::MergeCGI( params,
                                itC->url.GetParams(),
                                true );
      }
      pDataServer->SetParams( params );

      HostList::const_reverse_iterator it;
      for( it = hostList->rbegin(); it != hostList->rend(); ++it )
        if( it->loadBalancer )
        {
          pLoadBalancer = new URL( it->url );
          break;
        }

      for( it = hostList->rbegin(); it != hostList->rend(); ++it )
        if( it->flags & kXR_recoverWrts )
        {
          pWrtRecoveryRedir = new URL( it->url );
          break;
        }
    }

    log->Debug( FileMsg, "[0x%x@%s] Open has returned with status %s",
                this, pFileUrl->GetURL().c_str(), status->ToStr().c_str() );

    if( pDataServer && !pDataServer->IsLocalFile() )
    {
      //------------------------------------------------------------------------
      // Check if we are using a secure connection
      //------------------------------------------------------------------------
      XrdCl::AnyObject isencobj;
      XrdCl::XRootDStatus st = XrdCl::DefaultEnv::GetPostMaster()->
                QueryTransport( *pDataServer, XRootDQuery::IsEncrypted, isencobj );
      if( st.IsOK() )
      {
        bool *isenc;
        isencobj.Get( isenc );
        pIsChannelEncrypted = *isenc;
        delete isenc;
      }
    }

    //--------------------------------------------------------------------------
    // We have failed
    //--------------------------------------------------------------------------
    pStatus = *status;
    if( !pStatus.IsOK() || !openInfo )
    {
      log->Debug( FileMsg, "[0x%x@%s] Error while opening at %s: %s",
                  this, pFileUrl->GetURL().c_str(), lastServer.c_str(),
                  pStatus.ToStr().c_str() );
      FailQueuedMessages( pStatus );
      pFileState = Error;

      //------------------------------------------------------------------------
      // Report to monitoring
      //------------------------------------------------------------------------
      Monitor *mon = DefaultEnv::GetMonitor();
      if( mon )
      {
        Monitor::ErrorInfo i;
        i.file   = pFileUrl;
        i.status = status;
        i.opCode = Monitor::ErrorInfo::ErrOpen;
        mon->Event( Monitor::EvErrIO, &i );
      }
    }
    //--------------------------------------------------------------------------
    // We have succeeded
    //--------------------------------------------------------------------------
    else
    {
      //------------------------------------------------------------------------
      // Store the response info
      //------------------------------------------------------------------------
      openInfo->GetFileHandle( pFileHandle );
      pSessionId = openInfo->GetSessionId();
      if( openInfo->GetStatInfo() )
      {
        delete pStatInfo;
        pStatInfo = new StatInfo( *openInfo->GetStatInfo() );
      }

      log->Debug( FileMsg, "[0x%x@%s] successfully opened at %s, handle: 0x%x, "
                  "session id: %ld", this, pFileUrl->GetURL().c_str(),
                  pDataServer->GetHostId().c_str(), *((uint32_t*)pFileHandle),
                  pSessionId );

      //------------------------------------------------------------------------
      // Inform the monitoring about opening success
      //------------------------------------------------------------------------
      gettimeofday( &pOpenTime, 0 );
      Monitor *mon = DefaultEnv::GetMonitor();
      if( mon )
      {
        Monitor::OpenInfo i;
        i.file       = pFileUrl;
        i.dataServer = pDataServer->GetHostId();
        i.oFlags     = pOpenFlags;
        i.fSize      = pStatInfo ? pStatInfo->GetSize() : 0;
        mon->Event( Monitor::EvOpen, &i );
      }

      //------------------------------------------------------------------------
      // Resend the queued messages if any
      //------------------------------------------------------------------------
      ReSendQueuedMessages();
      pFileState  = Opened;
    }
  }

  //----------------------------------------------------------------------------
  // Process the results of the closing operation
  //----------------------------------------------------------------------------
  void FileStateHandler::OnClose( const XRootDStatus *status )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    log->Debug( FileMsg, "[0x%x@%s] Close returned from %s with: %s", this,
                pFileUrl->GetURL().c_str(), pDataServer->GetHostId().c_str(),
                status->ToStr().c_str() );

    log->Dump( FileMsg, "[0x%x@%s] Items in the fly %d, queued for recovery %d",
               this, pFileUrl->GetURL().c_str(), pInTheFly.size(),
               pToBeRecovered.size() );

    MonitorClose( status );
    ResetMonitoringVars();

    pStatus    = *status;
    pFileState = Closed;
  }

  //----------------------------------------------------------------------------
  // Handle an error while sending a stateful message
  //----------------------------------------------------------------------------
  void FileStateHandler::OnStateError( XRootDStatus      *status,
                                       Message           *message,
                                       ResponseHandler   *userHandler,
                                       MessageSendParams &sendParams )
  {
    //--------------------------------------------------------------------------
    // It may be a redirection
    //--------------------------------------------------------------------------
    if( !status->IsOK() && status->code == errRedirect && pFollowRedirects )
    {
      static const std::string root  = "root",  xroot  = "xroot", file = "file",
                               roots = "roots", xroots = "xroots";
      std::string msg = status->GetErrorMessage();
      if( !msg.compare( 0, root.size(),   root )  ||
          !msg.compare( 0, xroot.size(),  xroot ) ||
          !msg.compare( 0, file.size(),   file )  ||
          !msg.compare( 0, roots.size(),  roots ) ||
          !msg.compare( 0, xroots.size(), xroots ) )
      {
        OnStateRedirection( msg, message, userHandler, sendParams );
        return;
      }
    }

    //--------------------------------------------------------------------------
    // Handle error
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );
    pInTheFly.erase( message );

    log->Dump( FileMsg, "[0x%x@%s] File state error encountered. Message %s "
               "returned with %s", this, pFileUrl->GetURL().c_str(),
               message->GetDescription().c_str(), status->ToStr().c_str() );

    //--------------------------------------------------------------------------
    // Report to monitoring
    //--------------------------------------------------------------------------
    Monitor *mon = DefaultEnv::GetMonitor();
    if( mon )
    {
      Monitor::ErrorInfo i;
      i.file   = pFileUrl;
      i.status = status;

      ClientRequest *req = (ClientRequest*)message->GetBuffer();
      switch( req->header.requestid )
      {
        case kXR_read:    i.opCode = Monitor::ErrorInfo::ErrRead;   break;
        case kXR_readv:   i.opCode = Monitor::ErrorInfo::ErrReadV;  break;
        case kXR_pgread:  i.opCode = Monitor::ErrorInfo::ErrRead;   break;
        case kXR_write:   i.opCode = Monitor::ErrorInfo::ErrWrite;  break;
        case kXR_writev:  i.opCode = Monitor::ErrorInfo::ErrWriteV; break;
        case kXR_pgwrite: i.opCode = Monitor::ErrorInfo::ErrWrite;  break;
        default: i.opCode = Monitor::ErrorInfo::ErrUnc;
      }

      mon->Event( Monitor::EvErrIO, &i );
    }

    //--------------------------------------------------------------------------
    // The message is not recoverable
    // (message using a kernel buffer is not recoverable by definition)
    //--------------------------------------------------------------------------
    if( !IsRecoverable( *status ) || sendParams.kbuff )
    {
      log->Error( FileMsg, "[0x%x@%s] Fatal file state error. Message %s "
                 "returned with %s", this, pFileUrl->GetURL().c_str(),
                 message->GetDescription().c_str(), status->ToStr().c_str() );

      FailMessage( RequestData( message, userHandler, sendParams ), *status );
      delete status;
      return;
    }

    //--------------------------------------------------------------------------
    // Insert the message to the recovery queue and start the recovery
    // procedure if we don't have any more message in the fly
    //--------------------------------------------------------------------------
    pCloseReason = *status;
    RecoverMessage( RequestData( message, userHandler, sendParams ) );
    delete status;
  }

  //----------------------------------------------------------------------------
  // Handle stateful redirect
  //----------------------------------------------------------------------------
  void FileStateHandler::OnStateRedirection( const std::string &redirectUrl,
                                             Message           *message,
                                             ResponseHandler   *userHandler,
                                             MessageSendParams &sendParams )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    pInTheFly.erase( message );

    //--------------------------------------------------------------------------
    // Register the state redirect url and append the new cgi information to
    // the file URL
    //--------------------------------------------------------------------------
    if( !pStateRedirect )
    {
      std::ostringstream o;
      pStateRedirect = new URL( redirectUrl );
      URL::ParamsMap params = pFileUrl->GetParams();
      MessageUtils::MergeCGI( params,
                              pStateRedirect->GetParams(),
                              false );
      pFileUrl->SetParams( params );
    }

    RecoverMessage( RequestData( message, userHandler, sendParams ) );
  }

  //----------------------------------------------------------------------------
  // Handle stateful response
  //----------------------------------------------------------------------------
  void FileStateHandler::OnStateResponse( XRootDStatus *status,
                                          Message      *message,
                                          AnyObject    *response,
                                          HostList     */*urlList*/ )
  {
    Log    *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    log->Dump( FileMsg, "[0x%x@%s] Got state response for message %s",
               this, pFileUrl->GetURL().c_str(),
               message->GetDescription().c_str() );

    //--------------------------------------------------------------------------
    // Since this message may be the last "in-the-fly" and no recovery
    // is done if messages are in the fly, we may need to trigger recovery
    //--------------------------------------------------------------------------
    pInTheFly.erase( message );
    RunRecovery();

    //--------------------------------------------------------------------------
    // Play with the actual response before returning it. This is a good
    // place to do caching in the future.
    //--------------------------------------------------------------------------
    ClientRequest *req = (ClientRequest*)message->GetBuffer();
    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // Cache the stat response
      //------------------------------------------------------------------------
      case kXR_stat:
      {
        StatInfo *info = 0;
        response->Get( info );
        delete pStatInfo;
        pStatInfo = new StatInfo( *info );
        break;
      }

      //------------------------------------------------------------------------
      // Handle read response
      //------------------------------------------------------------------------
      case kXR_read:
      {
        ++pRCount;
        pRBytes += req->read.rlen;
        break;
      }

      //------------------------------------------------------------------------
      // Handle read response
      //------------------------------------------------------------------------
      case kXR_pgread:
      {
        ++pRCount;
        pRBytes += req->pgread.rlen;
        break;
      }

      //------------------------------------------------------------------------
      // Handle readv response
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        ++pVRCount;
        size_t segs = req->header.dlen/sizeof(readahead_list);
        readahead_list *dataChunk = (readahead_list*)message->GetBuffer( 24 );
        for( size_t i = 0; i < segs; ++i )
          pVRBytes += dataChunk[i].rlen;
        pVSegs += segs;
        break;
      }

      //------------------------------------------------------------------------
      // Handle write response
      //------------------------------------------------------------------------
      case kXR_write:
      {
        ++pWCount;
        pWBytes += req->write.dlen;
        break;
      }

      //------------------------------------------------------------------------
      // Handle write response
      //------------------------------------------------------------------------
      case kXR_pgwrite:
      {
        ++pWCount;
        pWBytes += req->pgwrite.dlen;
        break;
      }

      //------------------------------------------------------------------------
      // Handle writev response
      //------------------------------------------------------------------------
      case kXR_writev:
      {
        ++pVWCount;
        size_t size = req->header.dlen/sizeof(readahead_list);
        XrdProto::write_list *wrtList =
            reinterpret_cast<XrdProto::write_list*>( message->GetBuffer( 24 ) );
        for( size_t i = 0; i < size; ++i )
          pVWBytes += wrtList[i].wlen;
        break;
      }
    };
  }

  //------------------------------------------------------------------------
  //! Tick
  //------------------------------------------------------------------------
  void FileStateHandler::Tick( time_t now )
  {
    if (pMutex.CondLock())
       {TimeOutRequests( now );
        pMutex.UnLock();
       }
  }

  //----------------------------------------------------------------------------
  // Declare timeout on requests being recovered
  //----------------------------------------------------------------------------
  void FileStateHandler::TimeOutRequests( time_t now )
  {
    if( !pToBeRecovered.empty() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Dump( FileMsg, "[0x%x@%s] Got a timer event", this,
                 pFileUrl->GetURL().c_str() );
      RequestList::iterator it;
      JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
      for( it = pToBeRecovered.begin(); it != pToBeRecovered.end(); )
      {
        if( it->params.expires <= now )
        {
          jobMan->QueueJob( new ResponseJob(
                              it->handler,
                              new XRootDStatus( stError, errOperationExpired ),
                              0, it->params.hostList ) );
          it = pToBeRecovered.erase( it );
        }
        else
          ++it;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Called in the child process after the fork
  //----------------------------------------------------------------------------
  void FileStateHandler::AfterForkChild()
  {
    Log *log = DefaultEnv::GetLog();

    if( pFileState == Closed || pFileState == Error )
      return;

    if( (IsReadOnly() && pDoRecoverRead) ||
        (!IsReadOnly() && pDoRecoverWrite) )
    {
      log->Debug( FileMsg, "[0x%x@%s] Putting the file in recovery state in "
                  "process %d", this, pFileUrl->GetURL().c_str(), getpid() );
      pFileState = Recovering;
      pInTheFly.clear();
      pToBeRecovered.clear();
    }
    else
      pFileState = Error;
  }

  //------------------------------------------------------------------------
  // Try other data server
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::TryOtherServer( uint16_t timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened || !pLoadBalancer )
      return XRootDStatus( stError, errInvalidOp );

    pFileState = Recovering;

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Reopen file at next data server.",
                this, pFileUrl->GetURL().c_str() );

    // merge CGI
    auto lbcgi = pLoadBalancer->GetParams();
    auto dtcgi = pDataServer->GetParams();
    MessageUtils::MergeCGI( lbcgi, dtcgi, false );
    // update tried CGI
    auto itr = lbcgi.find( "tried" );
    if( itr == lbcgi.end() )
      lbcgi["tried"] = pDataServer->GetHostName();
    else
    {
     std::string tried = itr->second;
     tried += "," + pDataServer->GetHostName();
     lbcgi["tried"] = tried;
    }
    pLoadBalancer->SetParams( lbcgi );

    return ReOpenFileAtServer( *pLoadBalancer, timeout );
  }

  //------------------------------------------------------------------------
  // Generic implementation of xattr operation
  //------------------------------------------------------------------------
  template<typename T>
  Status FileStateHandler::XAttrOperationImpl( kXR_char              subcode,
                                               kXR_char              options,
                                               const std::vector<T> &attrs,
                                               ResponseHandler      *handler,
                                               uint16_t              timeout )
  {
    //--------------------------------------------------------------------------
    // Issue a new fattr request
    //--------------------------------------------------------------------------
    Message            *msg;
    ClientFattrRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_fattr;
    req->subcode   = subcode;
    req->numattr   = attrs.size();
    req->options   = options;
    memcpy( req->fhandle, pFileHandle, 4 );
    XRootDStatus st = MessageUtils::CreateXAttrBody( msg, attrs );
    if( !st.IsOK() ) return st;

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Send a message to a host or put it in the recovery queue
  //----------------------------------------------------------------------------
  Status FileStateHandler::SendOrQueue( const URL         &url,
                                        Message           *msg,
                                        ResponseHandler   *handler,
                                        MessageSendParams &sendParams )
  {
    //--------------------------------------------------------------------------
    // Recovering
    //--------------------------------------------------------------------------
    if( pFileState == Recovering )
    {
      return RecoverMessage( RequestData( msg, handler, sendParams ), false );
    }

    //--------------------------------------------------------------------------
    // Trying to send
    //--------------------------------------------------------------------------
    if( pFileState == Opened )
    {
      msg->SetSessionId( pSessionId );
      XRootDStatus st = IssueRequest( *pDataServer, msg, handler, sendParams );

      //------------------------------------------------------------------------
      // Invalid session id means that the connection has been broken while we
      // were idle so we haven't been informed about this fact earlier.
      //------------------------------------------------------------------------
      if( !st.IsOK() && st.code == errInvalidSession && IsRecoverable( st ) )
        return RecoverMessage( RequestData( msg, handler, sendParams ), false );

      if( st.IsOK() )
        pInTheFly.insert(msg);
      else
        delete handler;
      return st;
    }
    return Status( stError, errInvalidOp );
  }

  //----------------------------------------------------------------------------
  // Check if the stateful error is recoverable
  //----------------------------------------------------------------------------
  bool FileStateHandler::IsRecoverable( const XRootDStatus &status ) const
  {
    if( status.code == errSocketError || status.code == errInvalidSession ||
        status.code == errTlsError )
    {
      if( IsReadOnly() && !pDoRecoverRead )
        return false;

      if( !IsReadOnly() && !pDoRecoverWrite )
        return false;

      return true;
    }
    return false;
  }

  //----------------------------------------------------------------------------
  // Check if the file is open for read only
  //----------------------------------------------------------------------------
  bool FileStateHandler::IsReadOnly() const
  {
    if( (pOpenFlags & kXR_open_read) && !(pOpenFlags & kXR_open_updt) &&
        !(pOpenFlags & kXR_open_apnd ) )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Recover a message
  //----------------------------------------------------------------------------
  Status FileStateHandler::RecoverMessage( RequestData rd,
                                           bool        callbackOnFailure )
  {
    pFileState = Recovering;

    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Putting message %s in the recovery list",
               this, pFileUrl->GetURL().c_str(),
               rd.request->GetDescription().c_str() );

    Status st = RunRecovery();
    if( st.IsOK() )
    {
      pToBeRecovered.push_back( rd );
      return st;
    }

    if( callbackOnFailure )
      FailMessage( rd, st );

    return st;
  }

  //----------------------------------------------------------------------------
  // Run the recovery procedure if appropriate
  //----------------------------------------------------------------------------
  Status FileStateHandler::RunRecovery()
  {
    if( pFileState != Recovering )
      return Status();

    if( !pInTheFly.empty() )
      return Status();

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Running the recovery procedure", this,
                pFileUrl->GetURL().c_str() );

    Status st;
    if( pStateRedirect )
    {
      SendClose( 0 );
      st = ReOpenFileAtServer( *pStateRedirect, 0 );
      delete pStateRedirect; pStateRedirect = 0;
    }
    else if( IsReadOnly() && pLoadBalancer )
      st = ReOpenFileAtServer( *pLoadBalancer, 0 );
    else
      st = ReOpenFileAtServer( *pDataServer, 0 );

    if( !st.IsOK() )
    {
      pFileState = Error;
      FailQueuedMessages( st );
    }

    return st;
  }

  //----------------------------------------------------------------------------
  // Send a close and ignore the response
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::SendClose( uint16_t timeout )
  {
    Message            *msg;
    ClientCloseRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_close;
    memcpy( req->fhandle, pFileHandle, 4 );

    XRootDTransport::SetDescription( msg );
    msg->SetSessionId( pSessionId );
    NullResponseHandler *handler = new NullResponseHandler();
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;

    MessageUtils::ProcessSendParams( params );

    return IssueRequest( *pDataServer, msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Re-open the current file at a given server
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::ReOpenFileAtServer( const URL &url, uint16_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Sending a recovery open command to %s",
               this, pFileUrl->GetURL().c_str(), url.GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Remove the kXR_delete and kXR_new flags, as we don't want the recovery
    // procedure to delete a file that has been partially updated or fail it
    // because a partially uploaded file already exists.
    //--------------------------------------------------------------------------
    if (pOpenFlags & kXR_delete)
    {
      pOpenFlags &= ~kXR_delete;
      pOpenFlags |=  kXR_open_updt;
    }

    pOpenFlags &= ~kXR_new;

    Message           *msg;
    ClientOpenRequest *req;
    URL u = url;

    if( url.GetPath().empty() )
      u.SetPath( pFileUrl->GetPath() );

    std::string path = u.GetPathWithFilteredParams();
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_open;
    req->mode      = pOpenMode;
    req->options   = pOpenFlags;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    // the handler has been removed from the queue
    // (because we are here) so we can destroy it
    if( pReOpenHandler )
    {
      // in principle this should not happen because reopen
      // is triggered only from StateHandler (Stat, Write, Read, etc.)
      // but not from Open itself but it is better to be on the save side
      pReOpenHandler->Destroy();
      pReOpenHandler = 0;
    }
    // create a new reopen handler
    // (it is not assigned to 'pReOpenHandler' in order not to bump the reference counter
    //  until we know that 'SendMessage' was successful)
    ResponseHandlerHolder *openHandler = new ResponseHandlerHolder( new OpenHandler( this, 0 ) );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    //--------------------------------------------------------------------------
    // Issue the open request
    //--------------------------------------------------------------------------
    XRootDStatus st = IssueRequest( url, msg, openHandler, params );

    // if there was a problem destroy the open handler
    if( !st.IsOK() )
    {
      openHandler->Destroy();
    }
    // otherwise keep the reference
    else
    {
      pReOpenHandler = openHandler->Self();
    }
    return st;
  }

  //------------------------------------------------------------------------
  // Fail a message
  //------------------------------------------------------------------------
  void FileStateHandler::FailMessage( RequestData rd, XRootDStatus status )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Failing message %s with %s",
               this, pFileUrl->GetURL().c_str(),
               rd.request->GetDescription().c_str(),
               status.ToStr().c_str() );

    StatefulHandler *sh = dynamic_cast<StatefulHandler*>(rd.handler);
    if( !sh )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "[0x%x@%s] Internal error while recovering %s",
                  this, pFileUrl->GetURL().c_str(),
                  rd.request->GetDescription().c_str() );
      return;
    }

    JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
    ResponseHandler *userHandler = sh->GetUserHandler();
    jobMan->QueueJob( new ResponseJob(
                        userHandler,
                        new XRootDStatus( status ),
                        0, rd.params.hostList ) );

    delete sh;
  }

  //----------------------------------------------------------------------------
  // Fail queued messages
  //----------------------------------------------------------------------------
  void FileStateHandler::FailQueuedMessages( XRootDStatus status )
  {
    RequestList::iterator it;
    for( it = pToBeRecovered.begin(); it != pToBeRecovered.end(); ++it )
      FailMessage( *it, status );
    pToBeRecovered.clear();
  }

  //------------------------------------------------------------------------
  // Re-send queued messages
  //------------------------------------------------------------------------
  void FileStateHandler::ReSendQueuedMessages()
  {
    RequestList::iterator it;
    for( it = pToBeRecovered.begin(); it != pToBeRecovered.end(); ++it )
    {
      it->request->SetSessionId( pSessionId );
      ReWriteFileHandle( it->request );
      XRootDStatus st = IssueRequest( *pDataServer, it->request,
                                 it->handler, it->params );
      if( !st.IsOK() )
        FailMessage( *it, st );
    }
    pToBeRecovered.clear();
  }

  //------------------------------------------------------------------------
  // Re-write file handle
  //------------------------------------------------------------------------
  void FileStateHandler::ReWriteFileHandle( Message *msg )
  {
    ClientRequestHdr *hdr = (ClientRequestHdr*)msg->GetBuffer();
    switch( hdr->requestid )
    {
      case kXR_read:
      {
        ClientReadRequest *req = (ClientReadRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_write:
      {
        ClientWriteRequest *req = (ClientWriteRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_sync:
      {
        ClientSyncRequest *req = (ClientSyncRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_truncate:
      {
        ClientTruncateRequest *req = (ClientTruncateRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_readv:
      {
        ClientReadVRequest *req = (ClientReadVRequest*)msg->GetBuffer();
        readahead_list *dataChunk = (readahead_list*)msg->GetBuffer( 24 );
        for( size_t i = 0; i < req->dlen/sizeof(readahead_list); ++i )
          memcpy( dataChunk[i].fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_writev:
      {
        ClientWriteVRequest *req =
            reinterpret_cast<ClientWriteVRequest*>( msg->GetBuffer() );
        XrdProto::write_list *wrtList =
            reinterpret_cast<XrdProto::write_list*>( msg->GetBuffer( 24 ) );
        size_t size = req->dlen / sizeof(XrdProto::write_list);
        for( size_t i = 0; i < size; ++i )
          memcpy( wrtList[i].fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_pgread:
      {
        ClientPgReadRequest *req = (ClientPgReadRequest*) msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_pgwrite:
      {
        ClientPgWriteRequest *req = (ClientPgWriteRequest*) msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
    }

    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Rewritten file handle for %s to 0x%x",
               this, pFileUrl->GetURL().c_str(), msg->GetDescription().c_str(),
               *((uint32_t*)pFileHandle) );
    XRootDTransport::SetDescription( msg );
  }

  //----------------------------------------------------------------------------
  // Dispatch monitoring information on close
  //----------------------------------------------------------------------------
  void FileStateHandler::MonitorClose( const XRootDStatus *status )
  {
    Monitor *mon = DefaultEnv::GetMonitor();
    if( mon )
    {
      Monitor::CloseInfo i;
      i.file = pFileUrl;
      i.oTOD = pOpenTime;
      gettimeofday( &i.cTOD, 0 );
      i.rBytes  = pRBytes;
      i.vrBytes = pVRBytes;
      i.wBytes  = pWBytes;
      i.vwBytes = pVWBytes;
      i.vSegs   = pVSegs;
      i.rCount  = pRCount;
      i.vCount  = pVRCount;
      i.wCount  = pWCount;
      i.status  = status;
      mon->Event( Monitor::EvClose, &i );
    }
  }

  XRootDStatus FileStateHandler::IssueRequest( const URL         &url,
                                               Message           *msg,
                                               ResponseHandler   *handler,
                                               MessageSendParams &sendParams )
  {
    // first handle Metalinks
    if( pUseVirtRedirector && url.IsMetalink() )
      return MessageUtils::RedirectMessage( url, msg, handler,
                                            sendParams, pLFileHandler );

    // than local file access
    if( url.IsLocalFile() )
      return pLFileHandler->ExecRequest( url, msg, handler, sendParams );

    // and finally ordinary XRootD requests
    return MessageUtils::SendMessage( url, msg, handler,
                                      sendParams, pLFileHandler );
  }

  //------------------------------------------------------------------------
  // Send a write request with payload being stored in a kernel buffer
  //------------------------------------------------------------------------
  XRootDStatus FileStateHandler::WriteKernelBuffer( uint64_t                               offset,
                                                    uint32_t                               length,
                                                    std::unique_ptr<XrdSys::KernelBuffer>  kbuff,
                                                    ResponseHandler                       *handler,
                                                    uint16_t                               timeout )
  {
    //--------------------------------------------------------------------------
    // Create the write request
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a write command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientWriteRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_write;
    req->offset     = offset;
    req->dlen       = length;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.kbuff           = kbuff.release();
    params.chunkList       = new ChunkList();

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );

    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }
}

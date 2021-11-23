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

#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClRequestSync.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClForkHandler.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClLocalFileTask.hh"
#include "XrdCl/XrdClZipListHandler.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <sys/stat.h>

#include <memory>
#include <algorithm>
#include <iterator>

namespace
{

  class LocalFS
  {
    public:

      XrdCl::XRootDStatus Stat( const std::string       &path,
                                XrdCl::ResponseHandler  *handler,
                                uint16_t                 timeout )
      {
        using namespace XrdCl;

        Log *log = DefaultEnv::GetLog();

        struct stat ssp;
        if( stat( path.c_str(), &ssp ) == -1 )
        {
          log->Error( FileMsg, "Stat: failed: %s", XrdSysE2T( errno ) );
          XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                  XProtocol::mapError( errno ),
                                                  XrdSysE2T( errno ) );
          return QueueTask( error, 0, handler );
        }

        // TODO support other mode options
        uint32_t flags = S_ISDIR( ssp.st_mode ) ? kXR_isDir : 0;

        std::ostringstream data;
        data << ssp.st_dev << " " << ssp.st_size << " " << flags << " "
            << ssp.st_mtime;
        log->Debug( FileMsg, data.str().c_str() );

        StatInfo *statInfo = new StatInfo();
        if( !statInfo->ParseServerResponse( data.str().c_str() ) )
        {
          log->Error( FileMsg, "Stat: ParseServerResponse failed." );
          delete statInfo;
          return QueueTask( new XRootDStatus( stError, errErrorResponse, kXR_FSError ),
                            0, handler );
        }

        AnyObject *resp = new AnyObject();
        resp->Set( statInfo );
        return QueueTask( new XRootDStatus(), resp, handler );
      }

      XrdCl::XRootDStatus Rm( const std::string       &path,
                              XrdCl::ResponseHandler  *handler,
                              uint16_t                 timeout )
      {
        using namespace XrdCl;

        Log *log = DefaultEnv::GetLog();
        if( unlink( path.c_str() ) )
        {
          log->Error( FileMsg, "Rm: failed: %s", XrdSysE2T( errno ) );
          XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                  XProtocol::mapError( errno ),
                                                  XrdSysE2T( errno ) );
          return QueueTask( error, 0, handler );
        }

        return QueueTask( new XRootDStatus(), 0, handler );
      }

      static LocalFS& Instance()
      {
        static LocalFS instance;
        return instance;
      }

    private:

      //------------------------------------------------------------------------
      // Private constructors
      //------------------------------------------------------------------------
      LocalFS() : jmngr( XrdCl::DefaultEnv::GetPostMaster()->GetJobManager() )
      {

      }

      //------------------------------------------------------------------------
      // Private copy constructors
      //------------------------------------------------------------------------
      LocalFS( const LocalFS& );

      //------------------------------------------------------------------------
      // Private assignment operator
      //------------------------------------------------------------------------
      LocalFS& operator=( const LocalFS& );

      //------------------------------------------------------------------------
      // QueueTask - queues error/success tasks for all operations.
      // Must always return stOK.
      // Is always creating the same HostList containing only localhost.
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus QueueTask( XrdCl::XRootDStatus *st, XrdCl::AnyObject *resp,
          XrdCl::ResponseHandler *handler )
      {
        using namespace XrdCl;

        // if it is simply the sync handler we can release the semaphore
        // and return there is no need to execute this in the thread-pool
        SyncResponseHandler *syncHandler =
            dynamic_cast<SyncResponseHandler*>( handler );
        if( syncHandler )
        {
          syncHandler->HandleResponse( st, resp );
          return XRootDStatus();
        }

        LocalFileTask *task = new LocalFileTask( st, resp, 0, handler );
        jmngr->QueueJob( task );
        return XRootDStatus();
      }

      XrdCl::JobManager *jmngr;

  };

  //----------------------------------------------------------------------------
  // Get delimiter for the opaque info
  //----------------------------------------------------------------------------
   char GetCgiDelimiter( bool &hasCgi )
   {
     if( !hasCgi )
     {
       hasCgi = true;
       return '?';
     }

     return '&';
   }
  //----------------------------------------------------------------------------
  // Filters out client specific CGI
  //----------------------------------------------------------------------------
  std::string FilterXrdClCgi( const std::string &path )
  {
    // first check if there's an opaque info at all
    size_t pos = path.find( '?' );
    if( pos == std::string::npos )
      return path;

    std::string filteredPath = path.substr( 0 , pos );
    std::string cgi = path.substr( pos + 1 );

    bool hasCgi = false;
    pos = 0;
    size_t xrdcl = std::string::npos;
    do
    {
      xrdcl = cgi.find( "xrdcl.", pos );

      if( xrdcl == std::string:: npos )
      {
        filteredPath += GetCgiDelimiter( hasCgi );
        filteredPath += cgi.substr( pos );
        pos = cgi.size();
      }
      else
      {
        if( xrdcl != pos )
        {
          filteredPath += GetCgiDelimiter( hasCgi );
          filteredPath += cgi.substr( pos, xrdcl - 1 - pos );
        }

        pos = cgi.find( '&', xrdcl );
        if( pos != std::string::npos )
          ++pos;
      }

    }
    while( pos < cgi.size() && pos != std::string::npos );

    return filteredPath;
  }

  //----------------------------------------------------------------------------
  //! Wrapper class used to delete FileSystem object
  //----------------------------------------------------------------------------
  class DeallocFSHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor and destructor
      //------------------------------------------------------------------------
      DeallocFSHandler( XrdCl::FileSystem *fs, ResponseHandler *userHandler ):
        pFS(fs), pUserHandler(userHandler) {}

      virtual ~DeallocFSHandler()
      {
        delete pFS;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        pUserHandler->HandleResponse(status, response);
        delete this;
      }

    private:
      XrdCl::FileSystem *pFS;
      ResponseHandler   *pUserHandler;
  };

  //----------------------------------------------------------------------------
  // Deep locate handler
  //----------------------------------------------------------------------------
  class DeepLocateHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      DeepLocateHandler( XrdCl::ResponseHandler   *handler,
                         const std::string        &path,
                         XrdCl::OpenFlags::Flags   flags,
                         time_t                    expires ):
        pFirstTime( true ),
        pPartial( false ),
        pOutstanding( 1 ),
        pHandler( handler ),
        pPath( path ),
        pFlags( flags ),
        pExpires(expires)
      {
        pLocations = new XrdCl::LocationInfo();
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      ~DeepLocateHandler()
      {
        delete pLocations;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        --pOutstanding;

        //----------------------------------------------------------------------
        // We've got an error, react accordingly
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          log->Dump( FileSystemMsg, "[0x%x@DeepLocate(%s)] Got error "
                     "response: %s", this, pPath.c_str(),
                     status->ToStr().c_str() );

          //--------------------------------------------------------------------
          // We have failed with the first request
          //--------------------------------------------------------------------
          if( pFirstTime )
          {
            log->Debug( FileSystemMsg, "[0x%x@DeepLocate(%s)] Failed to get "
                        "the initial location list: %s", this, pPath.c_str(),
                        status->ToStr().c_str() );
            pHandler->HandleResponse( status, response );
            scopedLock.UnLock();
            delete this;
            return;
          }

          pPartial = true;

          //--------------------------------------------------------------------
          // We have no more outstanding requests, so let give to the client
          // what we have
          //--------------------------------------------------------------------
          if( !pOutstanding )
          {
            log->Debug( FileSystemMsg, "[0x%x@DeepLocate(%s)] No outstanding "
                        "requests, give out what we've got", this,
                        pPath.c_str() );
            scopedLock.UnLock();
            HandleFinalResponse();
          }
          delete status;
          return;
        }
        pFirstTime = false;

        //----------------------------------------------------------------------
        // Extract the answer
        //----------------------------------------------------------------------
        LocationInfo *info = 0;
        response->Get( info );
        LocationInfo::Iterator it;

        log->Dump( FileSystemMsg, "[0x%x@DeepLocate(%s)] Got %d locations",
                   this, pPath.c_str(), info->GetSize() );

        for( it = info->Begin(); it != info->End(); ++it )
        {
          //--------------------------------------------------------------------
          // Add the location to the list
          //--------------------------------------------------------------------
          if( it->IsServer() )
          {
            pLocations->Add( *it );
            continue;
          }

          //--------------------------------------------------------------------
          // Ask the manager for the location of servers
          //--------------------------------------------------------------------
          if( it->IsManager() )
          {
            ++pOutstanding;
            FileSystem *fs = new FileSystem( it->GetAddress() );
            if( pOutstanding == 0 || // protect against overflow, short circuiting
                                     // will make sure the other part won't be executed
                !fs->Locate( pPath, pFlags, new DeallocFSHandler(fs, this),
                             pExpires-::time(0)).IsOK() )
            {
              --pOutstanding;
              pPartial = true;
              delete fs;
            }
          }
        }

        //----------------------------------------------------------------------
        // Clean up and check if we have anything else to do
        //----------------------------------------------------------------------
        delete response;
        delete status;
        if( !pOutstanding )
        {
          scopedLock.UnLock();
          HandleFinalResponse();
        }
      }

      //------------------------------------------------------------------------
      // Build the response for the client
      //------------------------------------------------------------------------
      void HandleFinalResponse()
      {
        using namespace XrdCl;

        //----------------------------------------------------------------------
        // Nothing found
        //----------------------------------------------------------------------
        if( !pLocations->GetSize() )
        {
          pHandler->HandleResponse( new XRootDStatus( stError, errErrorResponse,
                                                   kXR_NotFound,
                                                   "No valid location found" ),
                                    0 );
        }
        //----------------------------------------------------------------------
        // We return an answer
        //----------------------------------------------------------------------
        else
        {
          AnyObject *obj = new AnyObject();
          obj->Set( pLocations );
          pLocations = 0;
          XRootDStatus *st = new XRootDStatus();
          if( pPartial ) st->code = suPartial;
          pHandler->HandleResponse( st, obj );
        }
        delete this;
      }

    private:
      bool                      pFirstTime;
      bool                      pPartial;
      uint16_t                  pOutstanding;
      XrdCl::ResponseHandler   *pHandler;
      XrdCl::LocationInfo      *pLocations;
      std::string               pPath;
      XrdCl::OpenFlags::Flags   pFlags;
      time_t                    pExpires;
      XrdSysMutex               pMutex;
  };

  //----------------------------------------------------------------------------
  // Handle stat results for a dirlist request
  //----------------------------------------------------------------------------
  class DirListStatHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      DirListStatHandler( XrdCl::DirectoryList *list,
                          uint32_t              index,
                          XrdCl::RequestSync   *sync ):
        pList( list ),
        pIndex( index ),
        pSync( sync )
      {
      }

      //------------------------------------------------------------------------
      // Check if we were successful and if so put the StatInfo object
      // in the appropriate entry info
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        if( !status->IsOK() )
        {
          delete status;
          pSync->TaskDone( false );
          delete this;
          return;
        }

        XrdCl::StatInfo *info = 0;
        response->Get( info );
        response->Set( (char*) 0 );
        pList->At( pIndex )->SetStatInfo( info );
        delete status;
        delete response;
        pSync->TaskDone();
        delete this;
      }

    private:
      XrdCl::DirectoryList *pList;
      uint32_t              pIndex;
      XrdCl::RequestSync   *pSync;
  };

  //----------------------------------------------------------------------------
  // Recursive dirlist common context for all handlers
  //----------------------------------------------------------------------------
  struct RecursiveDirListCtx
  {
      RecursiveDirListCtx( const XrdCl::URL &url, const std::string &path,
                           XrdCl::DirListFlags::Flags flags,
                           XrdCl::ResponseHandler *handler, time_t expires ) :
                             finalst( 0 ), pending( 1 ),
                             dirList( new XrdCl::DirectoryList() ), expires( expires ),
                             handler( handler ), flags( flags ),
                             fs( new XrdCl::FileSystem( url ) )
      {
        dirList->SetParentName( path );
      }

      ~RecursiveDirListCtx()
      {
        delete finalst;
        delete dirList;
        delete fs;
      }

      void UpdateStatus( const XrdCl::XRootDStatus &st )
      {
        using namespace XrdCl;

        if( !finalst )
        {
          finalst = st.IsOK() ? new XRootDStatus() : new XRootDStatus( st );
          return;
        }

        // if they disagree set the status to partial
        if( ( finalst->IsOK() && !st.IsOK() ) ||
            ( !finalst->IsOK() && st.IsOK() ) )
          *finalst = XRootDStatus( stOK, suPartial );
      }

      XrdCl::XRootDStatus        *finalst;
      int                         pending;
      XrdCl::DirectoryList       *dirList;
      time_t                      expires;
      XrdCl::ResponseHandler     *handler;
      XrdCl::DirListFlags::Flags  flags;
      XrdCl::FileSystem          *fs;
      XrdSysMutex                 mtx;
  };

  //----------------------------------------------------------------------------
  // Handle results for a recursive dirlist request
  //----------------------------------------------------------------------------
  class RecursiveDirListHandler: public XrdCl::ResponseHandler
  {
    public:

      RecursiveDirListHandler( const XrdCl::URL &url,
                               const std::string &path,
                               XrdCl::DirListFlags::Flags flags,
                               XrdCl::ResponseHandler *handler,
                               time_t timeout )
      {
        time_t expires = 0;
        if( timeout )
          expires = ::time( 0 ) + timeout;
        pCtx = new RecursiveDirListCtx( url, path, flags,
                                        handler, expires );
      }

      RecursiveDirListHandler( RecursiveDirListCtx *ctx ) : pCtx( ctx )
      {

      }

      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        using namespace XrdCl;

        Log *log = DefaultEnv::GetLog();
        bool finalrsp = !( status->IsOK() && status->code == XrdCl::suContinue );
        XrdSysMutexHelper scoped( pCtx->mtx );

        // check if we have to continue with the same handler (the response
        // has been chunked), if not we can decrement the number of pending
        // DieLists
        if( finalrsp )
          --pCtx->pending;

        pCtx->UpdateStatus( *status );

        if( status->IsOK() )
        {
          // get the response
          DirectoryList *dirList = 0;
          response->Get( dirList );

          std::string parent = pCtx->dirList->GetParentName();

          DirectoryList::Iterator itr;
          for( itr = dirList->Begin(); itr != dirList->End(); ++itr )
          {
            DirectoryList::ListEntry *entry = *itr;
            StatInfo *info = entry->GetStatInfo();
            if( !info )
            {
              log->Error( FileMsg, "Recursive directory list operation for %s failed: "
                          "kXR_dirlist with stat operation not supported.",
                          parent.c_str() );
              pCtx->UpdateStatus( XRootDStatus( stError, errNotSupported ) );
              continue;
            }
            std::string path = dirList->GetParentName() + entry->GetName();

            // add new entry to the result
            path = path.substr( parent.size() );
            entry->SetStatInfo( 0 ); // StatInfo is no longer owned by dirList
            DirectoryList::ListEntry *e =
                new DirectoryList::ListEntry( entry->GetHostAddress(), path, info );
            pCtx->dirList->Add( e );

            // if it's a directory do a recursive call
            if( info->TestFlags( StatInfo::IsDir ) )
            {
              // bump the pending counter
              ++pCtx->pending;
              // switch of the recursive flag, we will
              // provide the respective handler ourself,
              // make sure that stat is on
              DirListFlags::Flags flags = ( pCtx->flags & (~DirListFlags::Recursive) )
                                          | DirListFlags::Stat;
              // the recursive dir list handler
              RecursiveDirListHandler *handler = new RecursiveDirListHandler( pCtx );
              // timeout
              time_t timeout = 0;
              if( pCtx->expires )
              {
                timeout = pCtx->expires - ::time( 0 );
                if( timeout <= 0 )
                {
                  log->Error( FileMsg, "Recursive directory list operation for %s expired.",
                              parent.c_str() );
                  pCtx->UpdateStatus( XRootDStatus( stError, errOperationExpired ) );
                  break;
                }
              }
              // send the request
              std::string child = parent + path;
              XRootDStatus st = pCtx->fs->DirList( child, flags, handler, timeout );
              if( !st.IsOK() )
              {
                log->Error( FileMsg, "Recursive directory list operation for %s failed: ",
                            child.c_str(), st.ToString().c_str() );
                pCtx->UpdateStatus( st );
                continue;
              }
            }
          }
        }

        // if there are no more outstanding dirlist queries we can finalize the request
        if( pCtx->pending == 0 )
        {
          AnyObject *resp = new AnyObject();
          resp->Set( pCtx->dirList );
          pCtx->dirList = 0; // dirList is no longer owned by pCtx
          pCtx->handler->HandleResponse( pCtx->finalst, resp );
          pCtx->finalst = 0; // status is no longer owned by pCtx

          // finalize the common context
          scoped.UnLock();
          delete pCtx;
        }
        // if the user requested chunked response we give what we have to the user handler
        else if( status->IsOK() && ( pCtx->flags & DirListFlags::Chunked ) )
        {
          std::string parent = pCtx->dirList->GetParentName();
          AnyObject *resp = new AnyObject();
          resp->Set( pCtx->dirList );
          pCtx->dirList = new XrdCl::DirectoryList();
          pCtx->dirList->SetParentName( parent );
          pCtx->handler->HandleResponse( new XRootDStatus( stOK, suContinue ), resp );
        }

        // clean up the arguments
        delete status;
        delete response;
        // if we won't be continuing with the same handler, it can be deleted
        if( finalrsp )
          delete this;
      }

    private:

      RecursiveDirListCtx *pCtx;
  };

  //----------------------------------------------------------------------------
  // Exception for a merge dirlist handler
  //----------------------------------------------------------------------------
  struct MergeDirLsErr
  {
      MergeDirLsErr( XrdCl::XRootDStatus *&status, XrdCl::AnyObject *&response ) :
        status( status ), response( response )
      {
        status = 0; response = 0;
      }

      MergeDirLsErr() :
        status( new XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal ) ),
        response( 0 )
      {

      }

      XrdCl::XRootDStatus *status;
      XrdCl::AnyObject    *response;
  };



  //----------------------------------------------------------------------------
  // Handle results for a merge dirlist request
  //----------------------------------------------------------------------------
  class MergeDirListHandler: public XrdCl::ResponseHandler
  {
    public:

      MergeDirListHandler( bool allowChunked, XrdCl::ResponseHandler *handler ) :
        allowChunked( allowChunked ), pHandler( handler )
      {

      }

      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        XrdSysMutexHelper lck( mtx );

        bool finalrsp = !( status->IsOK() && status->code == XrdCl::suContinue );

        try
        {
          if( !status->IsOK() )
            throw MergeDirLsErr( status, response );

          if( !response )
            throw MergeDirLsErr();

          XrdCl::DirectoryList *dirlist = 0;
          response->Get( dirlist );

          if( !dirlist )
            throw MergeDirLsErr();

          if( allowChunked )
            MergeChunked( dirlist );
          else
            Merge( dirlist );

          response->Set( dirlist );
          pHandler->HandleResponse( status, response );
        }
        catch( const MergeDirLsErr &err )
        {
          delete status; delete response;
          pHandler->HandleResponse( err.status, err.response );
        }

        if( finalrsp )
        {
          lck.UnLock();
          delete this;
        }
      }

      void MergeChunked( XrdCl::DirectoryList *&response )
      {
        using namespace XrdCl;

        std::set<ListEntry*, less> unique;
        // set of unique list entries from the response
        std::set<ListEntry*, less> tmp( response->Begin(), response->End() );
        // all the unique list entries that were not reported so far
        std::set_difference( tmp.begin(),         tmp.end(),
                             uniquesofar.begin(), uniquesofar.end(),
                             std::inserter( unique, unique.end() ) );

        // we update the set of unique list entries that were already
        // reported to the user's handler
        for( auto itr = unique.begin(); itr != unique.end(); ++itr )
        {
          ListEntry *ent = *itr;
          if( !uniquesofar.count( ent ) )
          {
            StatInfo  *info   = ent->GetStatInfo() ? new StatInfo( *ent->GetStatInfo() ) : 0;
            ListEntry *newent = new ListEntry( ent->GetHostAddress(), ent->GetName(), info );
            uniquesofar.insert( newent );
          }
        }

        DirectoryList *dirlist = new DirectoryList();
        dirlist->SetParentName( response->GetParentName() );
        for( auto itr = unique.begin(); itr != unique.end(); ++itr )
        {
          ListEntry *entry = *itr;
          dirlist->Add( new ListEntry( entry->GetHostAddress(),
                                       entry->GetName(),
                                       entry->GetStatInfo() ) );
          entry->SetStatInfo( 0 );
        }

        delete response;
        response = dirlist;
      }

      static void Merge( XrdCl::DirectoryList *&response )
      {
        std::set<ListEntry*, less> unique( response->Begin(), response->End() );

        XrdCl::DirectoryList *dirlist = new XrdCl::DirectoryList();
        dirlist->SetParentName( response->GetParentName() );
        for( auto itr = unique.begin(); itr != unique.end(); ++itr )
        {
          ListEntry *entry = *itr;
          dirlist->Add( new ListEntry( entry->GetHostAddress(),
                                       entry->GetName(),
                                       entry->GetStatInfo() ) );
          entry->SetStatInfo( 0 );
        }

        delete response;
        response = dirlist;
}

    private:

      typedef XrdCl::DirectoryList::ListEntry ListEntry;

      struct less
      {
        bool operator() (const ListEntry *x, const ListEntry *y) const
        {
          if( x->GetName() != y->GetName() )
            return x->GetName() < y->GetName();

          const XrdCl::StatInfo *xStatInfo = x->GetStatInfo();
          const XrdCl::StatInfo *yStatInfo = y->GetStatInfo();

          if( xStatInfo == yStatInfo )
            return false;

          if( xStatInfo == 0 )
            return true;

          if( yStatInfo == 0 )
            return false;

          if( xStatInfo->GetSize() != yStatInfo->GetSize() )
            return xStatInfo->GetSize() < yStatInfo->GetSize();

          if( xStatInfo->GetFlags() != yStatInfo->GetFlags() )
            return xStatInfo->GetFlags() < yStatInfo->GetFlags();

          return false;
        }
      };

      bool                        allowChunked;
      XrdSysMutex                 mtx;
      std::set<ListEntry*, less>  uniquesofar;
      XrdCl::ResponseHandler     *pHandler;
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Wrapper class used to assign a load balancer
  //----------------------------------------------------------------------------
  class AssignLBHandler: public ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor and destructor
      //------------------------------------------------------------------------
      AssignLBHandler( FileSystemImpl *fs, ResponseHandler *userHandler ):
        pFS(fs), pUserHandler(userHandler) {}

      virtual ~AssignLBHandler() {}

      //------------------------------------------------------------------------
      // Response callback
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
                                            AnyObject    *response,
                                            HostList     *hostList );

    private:
      FileSystemImpl  *pFS;
      ResponseHandler *pUserHandler;
  };

  //----------------------------------------------------------------------------
  //! Wrapper class used to assign last URL
  //----------------------------------------------------------------------------
  class AssignLastURLHandler: public ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor and destructor
      //------------------------------------------------------------------------
      AssignLastURLHandler( FileSystemImpl *fs, ResponseHandler *userHandler ):
        pFS(fs), pUserHandler(userHandler) {}

      virtual ~AssignLastURLHandler() {}

      //------------------------------------------------------------------------
      // Response callback
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
                                            AnyObject    *response,
                                            HostList     *hostList );

    private:
      FileSystemImpl  *pFS;
      ResponseHandler *pUserHandler;
  };

  //----------------------------------------------------------------------------
  //! Implementation holding the data members
  //----------------------------------------------------------------------------
  struct FileSystemImpl
  {
    FileSystemImpl( const URL &url ) :
      pLoadBalancerLookupDone( false ),
      pFollowRedirects( true ),
      pUrl( new URL( url.GetURL() ) )
    {
    }

    ~FileSystemImpl()
    {
      delete pUrl;
    }

    //------------------------------------------------------------------------
    // Send a message in a locked environment
    //------------------------------------------------------------------------
    XRootDStatus Send( Message                 *msg,
                       ResponseHandler         *handler,
                       MessageSendParams       &params )
    {
      Log *log = DefaultEnv::GetLog();
      XrdSysMutexHelper scopedLock( pMutex );

      log->Dump( FileSystemMsg, "[0x%x@%s] Sending %s", this,
                 pUrl->GetHostId().c_str(), msg->GetDescription().c_str() );

      AssignLastURLHandler *lastUrlHandler = new AssignLastURLHandler( this, handler );
      handler = lastUrlHandler;

      AssignLBHandler *lbHandler = nullptr;
      if( !pLoadBalancerLookupDone && pFollowRedirects )
      {
        lbHandler = new AssignLBHandler( this, handler );
        handler = lbHandler;
      }

      params.followRedirects = pFollowRedirects;

      auto st = MessageUtils::SendMessage( *pUrl, msg, handler, params, 0 );
      if( !st.IsOK() )
      {
        delete lastUrlHandler;
        delete lbHandler;
      }

      return st;
    }

    //----------------------------------------------------------------------------
    // Assign a load balancer if it has not already been assigned
    //----------------------------------------------------------------------------
    void AssignLoadBalancer( const URL &url )
    {
      Log *log = DefaultEnv::GetLog();
      XrdSysMutexHelper scopedLock( pMutex );

      if( pLoadBalancerLookupDone )
        return;

      log->Dump( FileSystemMsg, "[0x%x@%s] Assigning %s as load balancer", this,
                 pUrl->GetHostId().c_str(), url.GetHostId().c_str() );

      delete pUrl;
      pUrl = new URL( url );
      pLoadBalancerLookupDone = true;
    }

    //----------------------------------------------------------------------------
    // Assign last URL
    //----------------------------------------------------------------------------
    void AssignLastURL( const URL &url )
    {
      Log *log = DefaultEnv::GetLog();
      XrdSysMutexHelper scopedLock( pMutex );

      log->Dump( FileSystemMsg, "[0x%x@%s] Assigning %s as last URL", this,
                 pUrl->GetHostId().c_str(), url.GetHostId().c_str() );

      pLastUrl.reset( new URL( url ) );
    }

    XrdSysMutex           pMutex;
    bool                  pLoadBalancerLookupDone;
    bool                  pFollowRedirects;
    URL                  *pUrl;
    std::unique_ptr<URL>  pLastUrl;
  };

  //------------------------------------------------------------------------
  // Response callback
  //------------------------------------------------------------------------
  void AssignLBHandler::HandleResponseWithHosts( XRootDStatus *status,
                                                 AnyObject    *response,
                                                 HostList     *hostList )
  {
    if( status->IsOK() )
    {
      HostList::reverse_iterator it;
      for( it = hostList->rbegin(); it != hostList->rend(); ++it )
        if( it->loadBalancer )
        {
          pFS->AssignLoadBalancer( it->url );
          break;
        }
    }

    bool finalrsp = !( status->IsOK() && status->code == suContinue );

    SyncResponseHandler * syncHandler = dynamic_cast<SyncResponseHandler*>( pUserHandler );
    if( !syncHandler )
      pUserHandler->HandleResponseWithHosts( status, response, hostList );

    if( finalrsp )
    {
      if( syncHandler )
        pUserHandler->HandleResponseWithHosts( status, response, hostList );
      delete this;
    }
  }

  //------------------------------------------------------------------------
  // Response callback
  //------------------------------------------------------------------------
  void AssignLastURLHandler::HandleResponseWithHosts( XRootDStatus *status,
                                                      AnyObject    *response,
                                                      HostList     *hostList )
  {
    if( status->IsOK() && hostList )
      pFS->AssignLastURL( hostList->front().url );

    bool finalrsp = !( status->IsOK() && status->code == suContinue );

    SyncResponseHandler *syncHandler = dynamic_cast<SyncResponseHandler*>( pUserHandler );
    if( !syncHandler )
      pUserHandler->HandleResponseWithHosts( status, response, hostList );

    if( finalrsp )
    {
      if( syncHandler )
        pUserHandler->HandleResponseWithHosts( status, response, hostList );
      delete this;
    }
  }

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  FileSystem::FileSystem( const URL &url, bool enablePlugIns ):
    pImpl( new FileSystemImpl( url ) ),
    pPlugIn(0)
  {
    //--------------------------------------------------------------------------
    // Check if we need to install a plug-in for this URL
    //--------------------------------------------------------------------------
    if( enablePlugIns )
    {
      Log *log = DefaultEnv::GetLog();
      std::string urlStr = url.GetURL();
      PlugInFactory *fact = DefaultEnv::GetPlugInManager()->GetFactory(urlStr);
      if( fact )
      {
        pPlugIn = fact->CreateFileSystem( urlStr );
        if( !pPlugIn )
        {
          log->Error( FileMsg, "Plug-in factory failed to produce a plug-in "
                      "for %s, continuing without one", urlStr.c_str() );
        }
      }
    }

    if( !pPlugIn )
      DefaultEnv::GetForkHandler()->RegisterFileSystemObject( this );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  FileSystem::~FileSystem()
  {
    if( !pPlugIn )
    {
      if( DefaultEnv::GetForkHandler() )
        DefaultEnv::GetForkHandler()->UnRegisterFileSystemObject( this );
    }

    delete pPlugIn;
    delete pImpl;
  }

  //----------------------------------------------------------------------------
  // Locate a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Locate( const std::string &path,
                                   OpenFlags::Flags   flags,
                                   ResponseHandler   *handler,
                                   uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Locate( path, flags, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message             *msg;
    ClientLocateRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid = kXR_locate;
    req->options   = flags;
    req->dlen      = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Locate a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Locate( const std::string  &path,
                                   OpenFlags::Flags    flags,
                                   LocationInfo      *&response,
                                   uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = Locate( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Locate a file, recursively locate all disk servers - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DeepLocate( const std::string &path,
                                       OpenFlags::Flags   flags,
                                       ResponseHandler   *handler,
                                       uint16_t           timeout )
  {
    return Locate( path, flags,
                   new DeepLocateHandler( handler, path, flags,
                                          ::time(0)+timeout ),
                   timeout );
  }

  //----------------------------------------------------------------------------
  // Locate a file, recursively locate all disk servers - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DeepLocate( const std::string  &path,
                                       OpenFlags::Flags    flags,
                                       LocationInfo      *&response,
                                       uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = DeepLocate( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Move a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Mv( const std::string &source,
                               const std::string &dest,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Mv( source, dest, handler, timeout );

    std::string fSource = FilterXrdClCgi( source );
    std::string fDest   = FilterXrdClCgi( dest );

    Message         *msg;
    ClientMvRequest *req;
    MessageUtils::CreateRequest( msg, req, fSource.length() + fDest.length()+1 );

    req->requestid = kXR_mv;
    req->dlen      = fSource.length() + fDest.length()+1;
    req->arg1len   = fSource.length();
    msg->Append( fSource.c_str(), fSource.length(), 24 );
    *msg->GetBuffer(24 + fSource.length()) = ' ';
    msg->Append( fDest.c_str(), fDest.length(), 25 + fSource.length() );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Move a directory or a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Mv( const std::string &source,
                               const std::string &dest,
                               uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Mv( source, dest, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain server information - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Query( QueryCode::Code  queryCode,
                                  const Buffer    &arg,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
  {
    if( pPlugIn )
      return pPlugIn->Query( queryCode, arg, handler, timeout );

    Message            *msg;
    ClientQueryRequest *req;
    MessageUtils::CreateRequest( msg, req, arg.GetSize() );

    req->requestid = kXR_query;
    req->infotype  = queryCode;
    req->dlen      = arg.GetSize();
    msg->Append( arg.GetBuffer(), arg.GetSize(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain server information - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Query( QueryCode::Code   queryCode,
                                  const Buffer     &arg,
                                  Buffer          *&response,
                                  uint16_t          timeout )
  {
    SyncResponseHandler handler;
    Status st = Query( queryCode, arg, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Truncate a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Truncate( const std::string &path,
                                     uint64_t           size,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Truncate( path, size, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message               *msg;
    ClientTruncateRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid = kXR_truncate;
    req->offset    = size;
    req->dlen      = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Truncate a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Truncate( const std::string &path,
                                     uint64_t           size,
                                     uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Truncate( path, size, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Remove a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Rm( const std::string &path,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Rm( path, handler, timeout );

    if( pImpl->pUrl->IsLocalFile() )
      return LocalFS::Instance().Rm( path, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message         *msg;
    ClientRmRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid = kXR_rm;
    req->dlen      = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Remove a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Rm( const std::string &path,
                               uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Rm( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Create a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::MkDir( const std::string &path,
                                  MkDirFlags::Flags  flags,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->MkDir( path, flags, mode, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message            *msg;
    ClientMkdirRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid  = kXR_mkdir;
    req->options[0] = flags;
    req->mode       = mode;
    req->dlen       = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Create a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::MkDir( const std::string &path,
                                  MkDirFlags::Flags  flags,
                                  Access::Mode       mode,
                                  uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = MkDir( path, flags, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Remove a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::RmDir( const std::string &path,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->RmDir( path, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message            *msg;
    ClientRmdirRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid  = kXR_rmdir;
    req->dlen       = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Remove a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::RmDir( const std::string &path,
                                  uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = RmDir( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Change access mode on a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::ChMod( const std::string &path,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->ChMod( path, mode, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message            *msg;
    ClientChmodRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid  = kXR_chmod;
    req->mode       = mode;
    req->dlen       = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Change access mode on a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::ChMod( const std::string &path,
                                  Access::Mode       mode,
                                  uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = ChMod( path, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Ping( ResponseHandler *handler,
                                 uint16_t        timeout )
  {
    if( pPlugIn )
      return pPlugIn->Ping( handler, timeout );

    Message           *msg;
    ClientPingRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_ping;
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Ping( uint16_t timeout  )
  {
    SyncResponseHandler handler;
    Status st = Ping( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Stat( const std::string &path,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Stat( path, handler, timeout );

    if( pImpl->pUrl->IsLocalFile() )
      return LocalFS::Instance().Stat( path, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message           *msg;
    ClientStatRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid  = kXR_stat;
    req->options    = 0;
    req->dlen       = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Stat( const std::string  &path,
                                 StatInfo          *&response,
                                 uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = Stat( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::StatVFS( const std::string &path,
                                    ResponseHandler   *handler,
                                    uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->StatVFS( path, handler, timeout );

    std::string fPath = FilterXrdClCgi( path );

    Message           *msg;
    ClientStatRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid  = kXR_stat;
    req->options    = kXR_vfs;
    req->dlen       = fPath.length();
    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::StatVFS( const std::string  &path,
                                    StatInfoVFS       *&response,
                                    uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = StatVFS( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Obtain server protocol information - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Protocol( ResponseHandler *handler,
                                     uint16_t         timeout )
  {
    if( pPlugIn )
      return pPlugIn->Protocol( handler, timeout );

    Message               *msg;
    ClientProtocolRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_protocol;
    req->clientpv  = kXR_PROTOCOLVERSION;
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain server protocol information - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Protocol( ProtocolInfo *&response,
                                     uint16_t       timeout )
  {
    SyncResponseHandler handler;
    Status st = Protocol( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // List entries of a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DirList( const std::string   &path,
                                    DirListFlags::Flags  flags,
                                    ResponseHandler     *handler,
                                    uint16_t             timeout )
  {
    if( pPlugIn )
      return pPlugIn->DirList( path, flags, handler, timeout );

    URL url = URL( path );
    std::string fPath = FilterXrdClCgi( path );

    if( flags & DirListFlags::Zip )
    {
      // stat the file to check if it is a directory or a file
      // the ZIP handler will take care of the rest
      ZipListHandler *zipHandler = new ZipListHandler( *pImpl->pUrl, path, flags, handler, timeout );
      XRootDStatus st = Stat( path, zipHandler, timeout );
      if( !st.IsOK() )
        delete zipHandler;
      return st;
    }

    Message           *msg;
    ClientDirlistRequest *req;
    MessageUtils::CreateRequest( msg, req, fPath.length() );

    req->requestid  = kXR_dirlist;
    req->dlen       = fPath.length();

    if( ( flags & DirListFlags::Stat ) || ( flags & DirListFlags::Recursive ) )
      req->options[0] = kXR_dstat;

    if( ( flags & DirListFlags::Cksm ) )
      req->options[0] = kXR_dstat | kXR_dcksm;

    if( flags & DirListFlags::Recursive )
      handler = new RecursiveDirListHandler( *pImpl->pUrl, url.GetPath(), flags, handler, timeout );

    if( flags & DirListFlags::Merge )
      handler = new MergeDirListHandler( flags & DirListFlags::Chunked, handler );

    msg->Append( fPath.c_str(), fPath.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    if( flags & DirListFlags::Chunked )
      params.chunkedResponse = true;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // List entries of a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DirList( const std::string    &path,
                                    DirListFlags::Flags   flags,
                                    DirectoryList       *&response,
                                    uint16_t              timeout )
  {
    //--------------------------------------------------------------------------
    // Chunked response is only possible for async DirList call
    //--------------------------------------------------------------------------
    if( flags & DirListFlags::Chunked )
      return XRootDStatus( stError, errNotSupported );

    //--------------------------------------------------------------------------
    // If the path ends with '.zip' extension add Zip flag
    //--------------------------------------------------------------------------
    static const std::string zip_sufix = ".zip";
    if( path.size() >= zip_sufix.size() &&
        std::equal( zip_sufix.rbegin(), zip_sufix.rend(), path.rbegin() ) )
      flags |= DirListFlags::Zip;

    //--------------------------------------------------------------------------
    // We do the deep locate and ask all the returned servers for the list
    //--------------------------------------------------------------------------
    if( flags & DirListFlags::Locate )
    {
      //------------------------------------------------------------------------
      // Locate all the disk servers holding the directory
      //------------------------------------------------------------------------
      LocationInfo *locations;
      std::string locatePath = "*"; locatePath += path;
      XRootDStatus st = DeepLocate( locatePath,
          OpenFlags::PrefName | OpenFlags::Compress | OpenFlags::IntentDirList, locations );

      if( !st.IsOK() )
        return st;

      if( locations->GetSize() == 0 )
      {
        delete locations;
        return XRootDStatus( stError, errNotFound );
      }

      //------------------------------------------------------------------------
      // Ask each server for a directory list
      //------------------------------------------------------------------------
      flags &= ~DirListFlags::Locate;
      FileSystem    *fs;
      DirectoryList *currentResp  = 0;
      uint32_t       errors       = 0;
      uint32_t       numLocations = locations->GetSize();
      bool           partial      = st.code == suPartial ? true : false;

      response = new DirectoryList();
      response->SetParentName( path );

      for( uint32_t i = 0; i < locations->GetSize(); ++i )
      {
        URL locationURL( locations->At(i).GetAddress() );
        // make sure the original protocol is preserved (root vs roots)
        locationURL.SetProtocol( pImpl->pUrl->GetProtocol() );
        fs = new FileSystem( locationURL );
        st = fs->DirList( path, flags, currentResp, timeout );
        if( !st.IsOK() )
        {
          ++errors;
          delete fs;
          continue;
        }

        if( st.code == suPartial )
          partial = true;

        DirectoryList::Iterator it;

        for( it = currentResp->Begin(); it != currentResp->End(); ++it )
        {
          response->Add( *it );
          *it = 0;
        }

        delete fs;
        delete currentResp;
        fs          = 0;
        currentResp = 0;
      }
      delete locations;

      if( flags & DirListFlags::Merge )
        MergeDirListHandler::Merge( response );

      if( errors || partial )
      {
        if( errors == numLocations )
          return st;
        return XRootDStatus( stOK, suPartial );
      }
      return XRootDStatus();
    };

    //--------------------------------------------------------------------------
    // We just ask the current server
    //--------------------------------------------------------------------------
    SyncResponseHandler handler;
    XRootDStatus st = DirList( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    st = MessageUtils::WaitForResponse( &handler, response );
    if( !st.IsOK() )
      return st;

    //--------------------------------------------------------------------------
    // Do the stats on all the entries if necessary.
    // If we already have the stat objects it means that the bulk stat has
    // succeeded.
    //--------------------------------------------------------------------------
    if( !(flags & DirListFlags::Stat) )
      return st;

    if( response->GetSize() && response->At(0)->GetStatInfo() )
      return st;

    uint32_t quota = response->GetSize() <= 1024 ? response->GetSize() : 1024;
    RequestSync sync( response->GetSize(), quota );
    for( uint32_t i = 0; i < response->GetSize(); ++i )
    {
      std::string fullPath = response->GetParentName()+response->At(i)->GetName();
      ResponseHandler *handler = new DirListStatHandler( response, i, &sync );
      st = Stat( fullPath, handler, timeout );
      if( !st.IsOK() )
      {
        sync.TaskDone( false );
        delete handler;
      }
      sync.WaitForQuota();
    }
    sync.WaitForAll();

    if( sync.FailureCount() )
      return XRootDStatus( stOK, suPartial );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Send info to the server - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::SendInfo( const std::string &info,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->SendInfo( info, handler, timeout );

    Message          *msg;
    ClientSetRequest *req;
    const char *prefix    = "monitor info ";
    size_t      prefixLen = strlen( prefix );
    MessageUtils::CreateRequest( msg, req, info.length()+prefixLen );

    req->requestid  = kXR_set;
    req->dlen       = info.length()+prefixLen;
    msg->Append( prefix, prefixLen, 24 );
    msg->Append( info.c_str(), info.length(), 24+prefixLen );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  //! Send info to the server - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::SendInfo( const std::string  &info,
                                     Buffer            *&response,
                                     uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = SendInfo( info, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Prepare one or more files for access - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Prepare( const std::vector<std::string> &fileList,
                                    PrepareFlags::Flags             flags,
                                    uint8_t                         priority,
                                    ResponseHandler                *handler,
                                    uint16_t                        timeout )
  {
    if( pPlugIn )
      return pPlugIn->Prepare( fileList, flags, priority, handler, timeout );

    std::vector<std::string>::const_iterator it;
    std::string                              list;
    for( it = fileList.begin(); it != fileList.end(); ++it )
    {
      list += *it;
      list += "\n";
    }
    list.erase( list.length()-1, 1 );

    Message              *msg;
    ClientPrepareRequest *req;
    MessageUtils::CreateRequest( msg, req, list.length() );

    req->requestid  = kXR_prepare;
    req->options    = 0xff & flags;
    req->optionX    = 0xffff & ( flags >> 8 );
    req->prty       = priority;
    req->dlen       = list.length();

    msg->Append( list.c_str(), list.length(), 24 );

    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //------------------------------------------------------------------------
  // Prepare one or more files for access - sync
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::Prepare( const std::vector<std::string>  &fileList,
                                    PrepareFlags::Flags              flags,
                                    uint8_t                          priority,
                                    Buffer                         *&response,
                                    uint16_t                         timeout )
  {
    SyncResponseHandler handler;
    Status st = Prepare( fileList, flags, priority, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //------------------------------------------------------------------------
  // Set extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::SetXAttr( const std::string           &path,
                                     const std::vector<xattr_t>  &attrs,
                                     ResponseHandler             *handler,
                                     uint16_t                     timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return XAttrOperationImpl( kXR_fattrSet, 0, path, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Set extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::SetXAttr( const std::string           &path,
                                     const std::vector<xattr_t>  &attrs,
                                     std::vector<XAttrStatus>    &result,
                                     uint16_t                     timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = SetXAttr( path, attrs, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttrStatus> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // Get extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::GetXAttr( const std::string               &path,
                                     const std::vector<std::string>  &attrs,
                                     ResponseHandler                 *handler,
                                     uint16_t                         timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return XAttrOperationImpl( kXR_fattrGet, 0, path, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Get extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::GetXAttr( const std::string               &path,
                                     const std::vector<std::string>  &attrs,
                                     std::vector<XAttr>              &result,
                                     uint16_t                         timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = GetXAttr( path, attrs, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttr> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // Delete extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::DelXAttr( const std::string               &path,
                                     const std::vector<std::string>  &attrs,
                                     ResponseHandler                *handler,
                                     uint16_t                        timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return XAttrOperationImpl( kXR_fattrDel, 0, path, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Delete extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::DelXAttr( const std::string               &path,
                                     const std::vector<std::string>  &attrs,
                                     std::vector<XAttrStatus>        &result,
                                     uint16_t                         timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = DelXAttr( path, attrs, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttrStatus> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // List extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::ListXAttr( const std::string         &path,
                                      ResponseHandler           *handler,
                                      uint16_t                   timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    static const std::vector<std::string> nothing;
    return XAttrOperationImpl( kXR_fattrList,  ClientFattrRequest::aData,
                               path, nothing, handler, timeout );
  }

  //------------------------------------------------------------------------
  // List extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::ListXAttr( const std::string    &path,
                                      std::vector<XAttr>   &result,
                                      uint16_t              timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = ListXAttr( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttr> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //----------------------------------------------------------------------------
  // Set file property
  //----------------------------------------------------------------------------
  bool FileSystem::SetProperty( const std::string &name,
                                const std::string &value )
  {
    if( pPlugIn )
      return pPlugIn->SetProperty( name, value );

    if( name == "FollowRedirects" )
    {
      if( value == "true" ) pImpl->pFollowRedirects = true;
      else pImpl->pFollowRedirects = false;
      return true;
    }
    return false;
  }

  //----------------------------------------------------------------------------
  // Get file property
  //----------------------------------------------------------------------------
  bool FileSystem::GetProperty( const std::string &name,
                                std::string &value ) const
  {
    if( pPlugIn )
      return pPlugIn->GetProperty( name, value );

    if( name == "FollowRedirects" )
    {
      if( pImpl->pFollowRedirects ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "LastURL" )
    {
      if( pImpl->pLastUrl )
      {
        value = pImpl->pLastUrl->GetURL();
        return true;
      }
      else return false;
    }

    return false;
  }

  //------------------------------------------------------------------------
  // Generic implementation of xattr operation
  //------------------------------------------------------------------------
  template<typename T>
  Status FileSystem::XAttrOperationImpl( kXR_char               subcode,
                                         kXR_char               options,
                                         const std::string     &path,
                                         const std::vector<T>  &attrs,
                                         ResponseHandler       *handler,
                                         uint16_t               timeout )
  {
    Message            *msg;
    ClientFattrRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_fattr;
    req->subcode   = subcode;
    req->options   = options;
    req->numattr   = attrs.size();
    memset( req->fhandle, 0, 4 );
    XRootDStatus st = MessageUtils::CreateXAttrBody( msg, attrs, path );
    if( !st.IsOK() ) return st;

    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );

    return pImpl->Send( msg, handler, params );
  }

  //------------------------------------------------------------------------
  // Lock the internal lock
  //------------------------------------------------------------------------
  void FileSystem::Lock()
  {
    pImpl->pMutex.Lock();
  }

  //------------------------------------------------------------------------
  // Unlock the internal lock
  //------------------------------------------------------------------------
  void FileSystem::UnLock()
  {
    pImpl->pMutex.UnLock();
  }
}

//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#ifndef XRDCK_RECORDER_HH_
#define XRDCK_RECORDER_HH_

#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdClAction.hh"

#include <mutex>
#include <fcntl.h>

namespace XrdCl
{
//------------------------------------------------------------------------------
//! XrdClFile plugin that arecords all user actions and server responses and
//! dumps the data into a csv file.
//------------------------------------------------------------------------------
class Recorder: public FilePlugIn
{
  //----------------------------------------------------------------------------
  //! Singleton object used for thread-safe writing the recored data into
  //! a csv file
  //----------------------------------------------------------------------------
  class Output
  {
    public:

      //------------------------------------------------------------------------
      //! Get instance, make sure the output file is open
      //! @return : single instance of Output object
      //------------------------------------------------------------------------
      inline static Output& Instance()
      {
        Output& output = Get();
        std::unique_lock<std::mutex> lck( output.mtx );
        if( !output.IsValid() )
        {
          if( !output.Open() )
            DefaultEnv::GetLog()->Error( AppMsg, "[Recorder] Failed to create the output file." );
        }
        return output;
      }

      //------------------------------------------------------------------------
      //! @return : single instance of Output object
      //------------------------------------------------------------------------
      inline static Output& Get()
      {
        static Output output;
        return output;
      }

      //------------------------------------------------------------------------
      // Record the user action
      // @param action : the action to be recorded
      // @return : true if the data was successful written to disk,
      //           false otherwise
      //------------------------------------------------------------------------
      bool Write( std::unique_ptr<Action> action )
      {
        std::unique_lock<std::mutex> lck( mtx );
        const std::string &entry = action->ToString();
        int btsWritten = 0;
        do
        {
          int rc = ::write( fd, entry.c_str(), entry.size() );
          if( rc < 0 )
          {
            DefaultEnv::GetLog()->Warning( AppMsg, "[Recorder] failed to record an action: %s", strerror( errno ) );
            return false;
          }
          else
            btsWritten += rc;
        }
        while( size_t( btsWritten ) < entry.size() );
        return true;;
      }

      //------------------------------------------------------------------------
      //! Open the csv files
      //! @param path : path to the csv file
      //! @return : true if the file was successfully opened, false otherwise
      //------------------------------------------------------------------------
      bool Open()
      {
        fd = open( path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644 );
        if( fd < 0 )
          DefaultEnv::GetLog()->Warning( AppMsg, "[Recorder] failed to open the output file: %s", strerror( errno ) );
        return ( fd >= 0 );
      }

      //------------------------------------------------------------------------
      //! @return : true if the csv file is open, false otherwise
      //------------------------------------------------------------------------
      inline bool IsValid()
      {
        return ( fd > 0 );
      }

      void SetPath( const std::string &path )
      {
        this->path = path;
      }

    private:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Output( ) : fd( -1 )
      {
      }

      //------------------------------------------------------------------------
      // Deleted copy/move constructors and assignment operators
      //------------------------------------------------------------------------
      Output( const Output& ) = delete;
      Output( Output&& ) = delete;
      Output& operator=( const Output& ) = delete;
      Output& operator=( Output&& ) = delete;

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Output()
      {
        if( fd >= 0 )
        {
          int rc = close( fd );
          if( rc < 0 )
            DefaultEnv::GetLog()->Warning( AppMsg, "[Recorder] failed to close the output file: %s", strerror( errno ) );
        }
      }

      std::mutex  mtx;   //< mutex guarding the writes
      int         fd;    //< the csv file descriptor
      std::string path;  //< path to the csv file
  };

  //----------------------------------------------------------------------------
  //! Completion handler recording user action / server response
  //----------------------------------------------------------------------------
  struct RecordHandler : public ResponseHandler
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //! @param output  : the object handling writes to csv file
    //! @param action  : user action to be recorded
    //! @param handler : user completion handler to be wrapped
    //--------------------------------------------------------------------------
    RecordHandler( Output                  &output,
                   std::unique_ptr<Action>  action,
                   ResponseHandler         *handler ) :
      output( output ),
      action( std::move( action ) ),
      handler( handler )
    {
    }

    //--------------------------------------------------------------------------
    //! Handle server response
    //! @param status   : operation status
    //! @param response : server response
    //! @param hostList : list of hosts involved in serving given request
    //--------------------------------------------------------------------------
    void HandleResponseWithHosts( XRootDStatus *status,
                                  AnyObject    *response,
                                  HostList     *hostList )
    {
      action->RecordResult( status, response );
      output.Write( std::move( action ) );
      handler->HandleResponseWithHosts( status, response, hostList );
      delete this;
    }

    //--------------------------------------------------------------------------
    //! Handle server response
    //! @param status   : operation status
    //! @param response : server response
    //--------------------------------------------------------------------------
    void HandleResponse( XRootDStatus *status,
                         AnyObject    *response )
    {
      action->RecordResult( status, response );
      output.Write( std::move( action ) );
      handler->HandleResponse( status, response );
      delete this;
    }

    Output                  &output;  //< the object handling writes to csv file
    std::unique_ptr<Action>  action;  //< user action
    ResponseHandler         *handler; //< user completion handler
  };


public:

  //----------------------------------------------------------------------------
  //! Create the output csv file
  //! @param cfgpath : path for the file to be created
  //----------------------------------------------------------------------------
  inline static void SetOutput( const std::string &cfgpath )
  {
    static const std::string defaultpath = "/tmp/xrdrecord.csv";
    const char *envpath = getenv( "XRD_RECORDERPATH" );
    std::string path = envpath ? envpath :
                       ( !cfgpath.empty() ? cfgpath : defaultpath );
    Output::Get().SetPath( path );
  }

  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  Recorder():
    file( false ),
    output( Output::Instance() )
  {
  }

  //----------------------------------------------------------------------------
  //! @return : true if this is a valid instance, false otherwise
  //----------------------------------------------------------------------------
  bool IsValid() const
  {
    return output.IsValid();
  }

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~Recorder()
  {
  }

  //----------------------------------------------------------------------------
  //! Open
  //----------------------------------------------------------------------------
  virtual XRootDStatus Open(const std::string& url,
                            OpenFlags::Flags flags,
                            Access::Mode mode,
                            ResponseHandler* handler,
                            uint16_t timeout)
  {
    std::unique_ptr<Action> ptr( new OpenAction( this, url, flags, mode, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Open( url, flags, mode, recHandler, timeout );
  }

  //----------------------------------------------------------------------------
  //! Close
  //----------------------------------------------------------------------------
  virtual XRootDStatus Close(ResponseHandler* handler,
                             uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new CloseAction( this, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Close( recHandler, timeout );
  }

  //----------------------------------------------------------------------------
  //! Stat
  //----------------------------------------------------------------------------
  virtual XRootDStatus Stat(bool             force,
                            ResponseHandler* handler,
                            uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new StatAction( this, force, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Stat(force, recHandler, timeout);
  }


  //----------------------------------------------------------------------------
  //! Read
  //----------------------------------------------------------------------------
  virtual XRootDStatus Read(uint64_t         offset,
                            uint32_t         size,
                            void*            buffer,
                            ResponseHandler* handler,
                            uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new ReadAction( this, offset, size, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Read( offset, size, buffer, recHandler, timeout );
  }

  //----------------------------------------------------------------------------
  //! Write
  //----------------------------------------------------------------------------
  virtual XRootDStatus Write(uint64_t         offset,
                             uint32_t         size,
                             const void*      buffer,
                             ResponseHandler* handler,
                             uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new WriteAction( this, offset, size, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Write( offset, size, buffer, recHandler, timeout );
  }

  //------------------------------------------------------------------------
  //! @see XrdCl:File PgRead
  //------------------------------------------------------------------------
  virtual XRootDStatus PgRead( uint64_t         offset,
                               uint32_t         size,
                               void            *buffer,
                               ResponseHandler *handler,
                               uint16_t         timeout )
  {
    std::unique_ptr<Action> ptr( new PgReadAction( this, offset, size, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.PgRead( offset, size, buffer, recHandler, timeout );
  }

  //------------------------------------------------------------------------
  //! @see XrdCl::File::PgWrite
  //------------------------------------------------------------------------
  virtual XRootDStatus PgWrite( uint64_t               offset,
                                uint32_t               size,
                                const void            *buffer,
                                std::vector<uint32_t> &cksums,
                                ResponseHandler       *handler,
                                uint16_t               timeout )
  {
    std::unique_ptr<Action> ptr( new PgWriteAction( this, offset, size, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.PgWrite( offset, size, buffer, cksums, recHandler, timeout );
  }

  //----------------------------------------------------------------------------
  //! Sync
  //----------------------------------------------------------------------------
  virtual XRootDStatus Sync(ResponseHandler* handler,
                            uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new SyncAction( this, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Sync( recHandler, timeout );
  }

  //----------------------------------------------------------------------------
  //! Truncate
  //----------------------------------------------------------------------------
  virtual XRootDStatus Truncate(uint64_t         size,
                                ResponseHandler* handler,
                                uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new TruncateAction( this, size, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Truncate(size, recHandler, timeout);
  }

  //----------------------------------------------------------------------------
  //! VectorRead
  //----------------------------------------------------------------------------
  virtual XRootDStatus VectorRead(const ChunkList& chunks,
                                  void*            buffer,
                                  ResponseHandler* handler,
                                  uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new VectorReadAction( this, chunks, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.VectorRead(chunks, buffer, recHandler, timeout);
  }

  //----------------------------------------------------------------------------
  //! VectorRead
  //----------------------------------------------------------------------------
  virtual XRootDStatus VectorWrite( const ChunkList &chunks,
                                    ResponseHandler *handler,
                                    uint16_t         timeout )
  {
    std::unique_ptr<Action> ptr( new VectorWriteAction( this, chunks, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.VectorWrite( chunks, recHandler, timeout );
  }

  //----------------------------------------------------------------------------
  //! Fcntl
  //----------------------------------------------------------------------------
  virtual XRootDStatus Fcntl(const Buffer&    arg,
                             ResponseHandler* handler,
                             uint16_t         timeout)
  {
    std::unique_ptr<Action> ptr( new FcntlAction( this, arg, timeout ) );
    RecordHandler *recHandler = new RecordHandler( output, std::move( ptr ), handler );
    return file.Fcntl(arg, recHandler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Visa
  //----------------------------------------------------------------------------
  virtual XRootDStatus Visa(ResponseHandler* handler,
                            uint16_t         timeout)
  {
    return file.Visa(handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! IsOpen
  //----------------------------------------------------------------------------
  virtual bool IsOpen() const
  {
    return file.IsOpen();
  }

  //----------------------------------------------------------------------------
  //! SetProperty
  //----------------------------------------------------------------------------
  virtual bool SetProperty(const std::string& name,
                           const std::string& value)
  {
    return file.SetProperty(name, value);
  }

  //----------------------------------------------------------------------------
  //! GetProperty
  //----------------------------------------------------------------------------
  virtual bool GetProperty(const std::string& name,
                           std::string& value) const
  {
    return file.GetProperty(name, value);
  }

private:

  File    file;   //< The file object that performs the actual operation
  Output &output; //< The object for writing the recorded actions
};

} // namespace XrdCl

#endif /* XRDCK_RECORDER_HH_ */

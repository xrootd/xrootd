//------------------------------------------------------------------------------
// Copyright (c) 2014 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClLog.hh"
#include "IdentityPlugIn.hh"
#include "TestEnv.hh"

using namespace XrdCl;
using namespace XrdClTests;

namespace
{
  //----------------------------------------------------------------------------
  // A plugin that forwards all the calls to XrdCl::File
  //----------------------------------------------------------------------------
  class IdentityFile: public XrdCl::FilePlugIn
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      IdentityFile()
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::IdentityFile" );
        pFile = new File( false );
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~IdentityFile()
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::~IdentityFile" );
        delete pFile;
      }

      //------------------------------------------------------------------------
      // Open
      //------------------------------------------------------------------------
      virtual XRootDStatus Open( const std::string &url,
                                 OpenFlags::Flags   flags,
                                 Access::Mode       mode,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Open" );
        return pFile->Open( url, flags, mode, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Close
      //------------------------------------------------------------------------
      virtual XRootDStatus Close( ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Close" );
        return pFile->Close( handler, timeout );
      }

      //------------------------------------------------------------------------
      // Stat
      //------------------------------------------------------------------------
      virtual XRootDStatus Stat( bool             force,
                                 ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Stat" );
        return pFile->Stat( force, handler, timeout );
      }


      //------------------------------------------------------------------------
      // Read
      //------------------------------------------------------------------------
      virtual XRootDStatus Read( uint64_t         offset,
                                 uint32_t         size,
                                 void            *buffer,
                                 ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Read" );
        return pFile->Read( offset, size, buffer, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Write
      //------------------------------------------------------------------------
      virtual XRootDStatus Write( uint64_t         offset,
                                  uint32_t         size,
                                  const void      *buffer,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Write" );
        return pFile->Write( offset, size, buffer, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Sync
      //------------------------------------------------------------------------
      virtual XRootDStatus Sync( ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Sync" );
        return pFile->Sync( handler, timeout );
      }

      //------------------------------------------------------------------------
      // Truncate
      //------------------------------------------------------------------------
      virtual XRootDStatus Truncate( uint64_t         size,
                                     ResponseHandler *handler,
                                     uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Truncate" );
        return pFile->Truncate( size, handler, timeout );
      }

      //------------------------------------------------------------------------
      // VectorRead
      //------------------------------------------------------------------------
      virtual XRootDStatus VectorRead( const ChunkList &chunks,
                                       void            *buffer,
                                       ResponseHandler *handler,
                                       uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::VectorRead" );
        return pFile->VectorRead( chunks, buffer, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Fcntl
      //------------------------------------------------------------------------
      virtual XRootDStatus Fcntl( const Buffer    &arg,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Fcntl" );
        return pFile->Fcntl( arg, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Visa
      //------------------------------------------------------------------------
      virtual XRootDStatus Visa( ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::Visa" );
        return pFile->Visa( handler, timeout );
      }

      //------------------------------------------------------------------------
      // IsOpen
      //------------------------------------------------------------------------
      virtual bool IsOpen() const
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::IsOpen" );
        return pFile->IsOpen();
      }

      //------------------------------------------------------------------------
      // EnableReadRecovery
      //------------------------------------------------------------------------
      virtual void EnableReadRecovery( bool enable )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::EnableReadRecovery" );
        pFile->EnableReadRecovery( enable );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::EnableWriteRecovery
      //------------------------------------------------------------------------
      virtual void EnableWriteRecovery( bool enable )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::EnableWriteRecovery" );
        pFile->EnableWriteRecovery( enable );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::GetDataServer
      //------------------------------------------------------------------------
      virtual std::string GetDataServer() const
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::GetDataServer" );
        return pFile->GetDataServer();
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::GetLastURL
      //------------------------------------------------------------------------
      virtual URL GetLastURL() const
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFile::GetLastURL" );
        return pFile->GetLastURL();
      }

    private:
      XrdCl::File *pFile;
  };

  //----------------------------------------------------------------------------
  // A plug-in that forwards all the calls to a XrdCl::FileSystem object
  //----------------------------------------------------------------------------
  class IdentityFileSystem: public FileSystemPlugIn
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      IdentityFileSystem( const std::string &url )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::IdentityFileSystem" );
        pFileSystem = new XrdCl::FileSystem( URL(url), false );
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~IdentityFileSystem()
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::~IdentityFileSysytem" );
        delete pFileSystem;
      }

      //------------------------------------------------------------------------
      // Locate
      //------------------------------------------------------------------------
      virtual XRootDStatus Locate( const std::string &path,
                                   OpenFlags::Flags   flags,
                                   ResponseHandler   *handler,
                                   uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Locate" );
        return pFileSystem->Locate( path, flags, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Mv
      //------------------------------------------------------------------------
      virtual XRootDStatus Mv( const std::string &source,
                               const std::string &dest,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Mv" );
        return pFileSystem->Mv( source, dest, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Query
      //------------------------------------------------------------------------
      virtual XRootDStatus Query( QueryCode::Code  queryCode,
                                  const Buffer    &arg,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Query" );
        return pFileSystem->Query( queryCode, arg, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Truncate
      //------------------------------------------------------------------------
      virtual XRootDStatus Truncate( const std::string &path,
                                     uint64_t           size,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Truncate" );
        return pFileSystem->Truncate( path, size, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Rm
      //------------------------------------------------------------------------
      virtual XRootDStatus Rm( const std::string &path,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Rm" );
        return pFileSystem->Rm( path, handler, timeout );
      }

      //------------------------------------------------------------------------
      // MkDir
      //------------------------------------------------------------------------
      virtual XRootDStatus MkDir( const std::string &path,
                                  MkDirFlags::Flags  flags,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::MkDir" );
        return pFileSystem->MkDir( path, flags, mode, handler, timeout );
      }

      //------------------------------------------------------------------------
      // RmDir
      //------------------------------------------------------------------------
      virtual XRootDStatus RmDir( const std::string &path,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::RmDir" );
        return pFileSystem->RmDir( path, handler, timeout );
      }

      //------------------------------------------------------------------------
      // ChMod
      //------------------------------------------------------------------------
      virtual XRootDStatus ChMod( const std::string &path,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::ChMod" );
        return pFileSystem->ChMod( path, mode, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Ping
      //------------------------------------------------------------------------
      virtual XRootDStatus Ping( ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Ping" );
        return pFileSystem->Ping( handler, timeout );
      }

      //------------------------------------------------------------------------
      // Stat
      //------------------------------------------------------------------------
      virtual XRootDStatus Stat( const std::string &path,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Stat" );
        return pFileSystem->Stat( path, handler, timeout );
      }

      //------------------------------------------------------------------------
      // StatVFS
      //------------------------------------------------------------------------
      virtual XRootDStatus StatVFS( const std::string &path,
                                    ResponseHandler   *handler,
                                    uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::StatVFS" );
        return pFileSystem->StatVFS( path, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Protocol
      //------------------------------------------------------------------------
      virtual XRootDStatus Protocol( ResponseHandler *handler,
                                     uint16_t         timeout = 0 )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Protocol" );
        return pFileSystem->Protocol( handler, timeout );
      }

      //------------------------------------------------------------------------
      // DirlList
      //------------------------------------------------------------------------
      virtual XRootDStatus DirList( const std::string   &path,
                                    DirListFlags::Flags  flags,
                                    ResponseHandler     *handler,
                                    uint16_t             timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::DirList" );
        return pFileSystem->DirList( path, flags, handler, timeout );
      }

      //------------------------------------------------------------------------
      // SendInfo
      //------------------------------------------------------------------------
      virtual XRootDStatus SendInfo( const std::string &info,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::SendInfo" );
        return pFileSystem->SendInfo( info, handler, timeout );
      }

      //------------------------------------------------------------------------
      // Prepare
      //------------------------------------------------------------------------
      virtual XRootDStatus Prepare( const std::vector<std::string> &fileList,
                                    PrepareFlags::Flags             flags,
                                    uint8_t                         priority,
                                    ResponseHandler                *handler,
                                    uint16_t                        timeout )
      {
        XrdCl::Log *log = TestEnv::GetLog();
        log->Debug( 1, "Calling IdentityFileSystem::Prepare" );
        return pFileSystem->Prepare( fileList, flags, priority, handler,
                                     timeout );
      }

    private:
      XrdCl::FileSystem *pFileSystem;
  };
}

namespace XrdClTests
{
  //----------------------------------------------------------------------------
  // Create a file plug-in for the given URL
  //----------------------------------------------------------------------------
  FilePlugIn *IdentityFactory::CreateFile( const std::string &url )
  {
    XrdCl::Log *log = TestEnv::GetLog();
    log->Debug( 1, "Creating an identity file plug-in" );
    return new IdentityFile();
  }

  //----------------------------------------------------------------------------
  // Create a file system plug-in for the given URL
  //----------------------------------------------------------------------------
  FileSystemPlugIn *IdentityFactory::CreateFileSystem( const std::string &url )
  {
    XrdCl::Log *log = TestEnv::GetLog();
    log->Debug( 1, "Creating an identity file system plug-in" );
    return new IdentityFileSystem( url );
  }
}


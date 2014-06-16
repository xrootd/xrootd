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

#ifndef __XRD_CL_PLUGIN_INTERFACE__
#define __XRD_CL_PLUGIN_INTERFACE__

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! An interface for file plug-ins
  //----------------------------------------------------------------------------
  class FilePlugIn
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~FilePlugIn() {}
      //------------------------------------------------------------------------
      //! @see XrdCl::File::Open
      //------------------------------------------------------------------------
      virtual XRootDStatus Open( const std::string &url,
                                 OpenFlags::Flags   flags,
                                 Access::Mode       mode,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
      {
        (void)url; (void)flags; (void)mode; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Close
      //------------------------------------------------------------------------
      virtual XRootDStatus Close( ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Stat
      //------------------------------------------------------------------------
      virtual XRootDStatus Stat( bool             force,
                                 ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        (void)force; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }


      //------------------------------------------------------------------------
      //! @see XrdCl::File::Read
      //------------------------------------------------------------------------
      virtual XRootDStatus Read( uint64_t         offset,
                                 uint32_t         size,
                                 void            *buffer,
                                 ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        (void)offset; (void)size; (void)buffer; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Write
      //------------------------------------------------------------------------
      virtual XRootDStatus Write( uint64_t         offset,
                                  uint32_t         size,
                                  const void      *buffer,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        (void)offset; (void)size; (void)buffer; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Sync
      //------------------------------------------------------------------------
      virtual XRootDStatus Sync( ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Truncate
      //------------------------------------------------------------------------
      virtual XRootDStatus Truncate( uint64_t         size,
                                     ResponseHandler *handler,
                                     uint16_t         timeout )
      {
        (void)size; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::VectorRead
      //------------------------------------------------------------------------
      virtual XRootDStatus VectorRead( const ChunkList &chunks,
                                       void            *buffer,
                                       ResponseHandler *handler,
                                       uint16_t         timeout )
      {
        (void)chunks; (void)buffer; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Fcntl
      //------------------------------------------------------------------------
      virtual XRootDStatus Fcntl( const Buffer    &arg,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        (void)arg; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::Visa
      //------------------------------------------------------------------------
      virtual XRootDStatus Visa( ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::IsOpen
      //------------------------------------------------------------------------
      virtual bool IsOpen() const
      {
        return false;
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::SetProperty
      //------------------------------------------------------------------------
      virtual bool SetProperty( const std::string &name,
                                const std::string &value )
      {
        (void)name; (void)value;
        return false;
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::GetProperty
      //------------------------------------------------------------------------
      virtual bool GetProperty( const std::string &name,
                                std::string &value ) const
      {
        (void)name; (void)value;
        return false;
      }
  };

  //----------------------------------------------------------------------------
  //! An interface for file plug-ins
  //----------------------------------------------------------------------------
  class FileSystemPlugIn
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~FileSystemPlugIn() {}

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Locate
      //------------------------------------------------------------------------
      virtual XRootDStatus Locate( const std::string &path,
                                   OpenFlags::Flags   flags,
                                   ResponseHandler   *handler,
                                   uint16_t           timeout )
      {
        (void)path; (void)flags; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Mv
      //------------------------------------------------------------------------
      virtual XRootDStatus Mv( const std::string &source,
                               const std::string &dest,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
      {
        (void)source; (void)dest; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Query
      //------------------------------------------------------------------------
      virtual XRootDStatus Query( QueryCode::Code  queryCode,
                                  const Buffer    &arg,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
      {
        (void)queryCode; (void)arg; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Truncate
      //------------------------------------------------------------------------
      virtual XRootDStatus Truncate( const std::string &path,
                                     uint64_t           size,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
      {
        (void)path; (void)size; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Rm
      //------------------------------------------------------------------------
      virtual XRootDStatus Rm( const std::string &path,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
      {
        (void)path; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::MkDir
      //------------------------------------------------------------------------
      virtual XRootDStatus MkDir( const std::string &path,
                                  MkDirFlags::Flags  flags,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
      {
        (void)path; (void)flags; (void)mode; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::RmDir
      //------------------------------------------------------------------------
      virtual XRootDStatus RmDir( const std::string &path,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
      {
        (void)path; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::ChMod
      //------------------------------------------------------------------------
      virtual XRootDStatus ChMod( const std::string &path,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
      {
        (void)path; (void)mode; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Ping
      //------------------------------------------------------------------------
      virtual XRootDStatus Ping( ResponseHandler *handler,
                                 uint16_t         timeout )
      {
        (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Stat
      //------------------------------------------------------------------------
      virtual XRootDStatus Stat( const std::string &path,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
      {
        (void)path; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::StatVFS
      //------------------------------------------------------------------------
      virtual XRootDStatus StatVFS( const std::string &path,
                                    ResponseHandler   *handler,
                                    uint16_t           timeout )
      {
        (void)path; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Protocol
      //------------------------------------------------------------------------
      virtual XRootDStatus Protocol( ResponseHandler *handler,
                                     uint16_t         timeout = 0 )
      {
        (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::DirlList
      //------------------------------------------------------------------------
      virtual XRootDStatus DirList( const std::string   &path,
                                    DirListFlags::Flags  flags,
                                    ResponseHandler     *handler,
                                    uint16_t             timeout )
      {
        (void)path; (void)flags; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::SendInfo
      //------------------------------------------------------------------------
      virtual XRootDStatus SendInfo( const std::string &info,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
      {
        (void)info; (void)handler; (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::Prepare
      //------------------------------------------------------------------------
      virtual XRootDStatus Prepare( const std::vector<std::string> &fileList,
                                    PrepareFlags::Flags             flags,
                                    uint8_t                         priority,
                                    ResponseHandler                *handler,
                                    uint16_t                        timeout )
      {
        (void)fileList; (void)flags; (void)priority; (void)handler;
        (void)timeout;
        return XRootDStatus( stError, errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::SetProperty
      //------------------------------------------------------------------------
      virtual bool SetProperty( const std::string &name,
                                const std::string &value )
      {
        (void)name; (void)value;
        return false;
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::FileSystem::GetProperty
      //------------------------------------------------------------------------
      virtual bool GetProperty( const std::string &name,
                                std::string &value ) const
      {
        (void)name; (void)value;
        return false;
      }
  };

  //----------------------------------------------------------------------------
  //! Plugin factory
  //----------------------------------------------------------------------------
  class PlugInFactory
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~PlugInFactory() {}

      //------------------------------------------------------------------------
      //! Create a file plug-in for the given URL
      //------------------------------------------------------------------------
      virtual FilePlugIn *CreateFile( const std::string &url ) = 0;

      //------------------------------------------------------------------------
      //! Create a file system plug-in for the given URL
      //------------------------------------------------------------------------
      virtual FileSystemPlugIn *CreateFileSystem( const std::string &url ) = 0;
  };
}

#endif // __XRD_CL_PLUGIN_INTERFACE__

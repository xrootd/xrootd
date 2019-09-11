//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
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
//------------------------------------------------------------------------------

#ifndef SRC_XRDCL_XRDCLZIPARCHIVEREADER_HH_
#define SRC_XRDCL_XRDCLZIPARCHIVEREADER_HH_

#include "XrdClXRootDResponses.hh"

namespace XrdCl
{

class ZipArchiveReaderImpl;
class File;

//----------------------------------------------------------------------------
//! A wrapper class for the XrdCl::File.
//!
//! It is an abstraction for a ZIP file containing multiple sub-files.
//! The class does not provide any unzip utilities, it just readjusts
//! the offset so a respective file inside of the archive can be read.
//! It is meant for ZIP archives containing uncompressed root files,
//! so a single file can be accessed without downloading the whole
//! archive.
//----------------------------------------------------------------------------
class ZipArchiveReader
{
  public:

    //------------------------------------------------------------------------
    //! Constructor.
    //!
    //! Wraps up the File object
    //------------------------------------------------------------------------
    ZipArchiveReader( File &archive );

    //------------------------------------------------------------------------
    //! Destructor.
    //------------------------------------------------------------------------
    virtual ~ZipArchiveReader();

    //------------------------------------------------------------------------
    //! Asynchronous open of a given ZIP archive for reading.
    //!
    //! During the open, the End-of-central-directory record
    //! and the Central-directory-file-headers records are
    //! being read and parsed.
    //!
    //! If the ZIP archive is smaller than the maximum size
    //! of the EOCD record the whole archive is being down-
    //! loaded and kept in local memory.
    //!
    //! @param url     : URL of the archive
    //! @param handler : the handler for the async operation
    //! @param timeout : the timeout of the async operation
    //!
    //! @return        : OK on success, error otherwise
    //------------------------------------------------------------------------
    XRootDStatus Open( const std::string &url, ResponseHandler *handler, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Synchronous open of a given ZIP archive for reading.
    //------------------------------------------------------------------------
    XRootDStatus Open( const std::string &url, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Async read.
    //!
    //! @param filename : name of the file that will the readout
    //! @param offset   : offset (relative for the given file)
    //! @param size     : size of the buffer
    //! @param buffer   : the readout buffer
    //! @param handler  : the handler for the async operation
    //! @param timeout  : the timeout of the async operation
    //!
    //! @return        : OK on success, error otherwise
    //------------------------------------------------------------------------
    XRootDStatus Read( const std::string &filename, uint64_t offset, uint32_t size, void *buffer, ResponseHandler *handler, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Sync read.
    //------------------------------------------------------------------------
    XRootDStatus Read( const std::string &filename, uint64_t offset, uint32_t size, void *buffer, uint32_t &bytesRead, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Bounds the reader to a file inside the archive.
    //------------------------------------------------------------------------
    XRootDStatus Bind( const std::string &filename );

    //------------------------------------------------------------------------
    //! Async bound read.
    //------------------------------------------------------------------------
    XRootDStatus Read( uint64_t offset, uint32_t size, void *buffer, ResponseHandler *handler, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Sync bound read.
    //------------------------------------------------------------------------
    XRootDStatus Read( uint64_t offset, uint32_t size, void *buffer, uint32_t &bytesRead, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Sync list
    //------------------------------------------------------------------------
    XRootDStatus List( DirectoryList *&list );

    //------------------------------------------------------------------------
    //! Async close.
    //!
    //! @param handler : the handler for the async operation
    //! @param timeout : the timeout of the async operation
    //!
    //! @return        : OK on success, error otherwise
    //------------------------------------------------------------------------
    XRootDStatus Close( ResponseHandler *handler, uint16_t timeout = 0 );

    //------------------------------------------------------------------------
    //! Sync close.
    //------------------------------------------------------------------------
    XRootDStatus Close( uint16_t timeout  = 0 );

    //------------------------------------------------------------------------
    //! Gets the size of the given file
    //!
    //! @param filename : the name of the file
    //!
    //! @return         : the size of the file as in CDFH record
    //------------------------------------------------------------------------
    XRootDStatus GetSize( const std::string &filename, uint64_t &size ) const;

    //------------------------------------------------------------------------
    //! Check if the archive is open
    //------------------------------------------------------------------------
    bool IsOpen() const;

    //------------------------------------------------------------------------
    //! The CRC32 checksum as in the ZIP archive
    //------------------------------------------------------------------------
    XRootDStatus ZCRC32( const std::string &filename, std::string &checksum );

    //------------------------------------------------------------------------
    //! The CRC32 checksum as in the ZIP archive (bound version)
    //------------------------------------------------------------------------
    XRootDStatus ZCRC32( std::string &checksum );

  private:

    //------------------------------------------------------------------------
    //! Pointer to the implementation.
    //------------------------------------------------------------------------
    ZipArchiveReaderImpl *pImpl;
};

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLZIPARCHIVEREADER_HH_ */

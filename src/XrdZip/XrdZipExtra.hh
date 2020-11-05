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

#ifndef SRC_XRDZIP_XRDZIPEXTRA_HH_
#define SRC_XRDZIP_XRDZIPEXTRA_HH_

#include "XrdZip/XrdZipUtils.hh"

namespace XrdZip
{
  //---------------------------------------------------------------------------
  // A data structure for the ZIP64 extra field
  //---------------------------------------------------------------------------
  struct ZipExtra
  {
    //-------------------------------------------------------------------------
    //! Constructor from file size
    //-------------------------------------------------------------------------
    ZipExtra( uint64_t fileSize )
    {
      offset = 0;
      nbDisk = 0;
      if ( fileSize >= ovrflw<uint32_t>::value )
      {
        dataSize = 16;
        uncompressedSize = fileSize;
        compressedSize = fileSize;
        totalSize = dataSize + 4;
      }
      else
      {
        dataSize = 0;
        uncompressedSize = 0;
        compressedSize = 0;
        totalSize = 0;
      }
    }

    //-------------------------------------------------------------------------
    //! Constructor from another extra + offset
    //-------------------------------------------------------------------------
    ZipExtra( ZipExtra *extra, uint64_t offset )
    {
      nbDisk = 0;
      uncompressedSize = extra->uncompressedSize;
      compressedSize = extra->compressedSize;
      dataSize = extra->dataSize;
      totalSize = extra->totalSize;
      if ( offset >= ovrflw<uint32_t>::value )
      {
        this->offset = offset;
        dataSize += 8;
        totalSize = dataSize + 4;
      }
      else
        this->offset = 0;
    }

    //-------------------------------------------------------------------------
    //! Serialize the extra field into a buffer
    //-------------------------------------------------------------------------
    void Serialize( buffer_t &buffer )
    {
      if ( totalSize > 0 )
      {
        copy_bytes( headerID, buffer );
        copy_bytes( dataSize, buffer );
        if ( uncompressedSize > 0)
        {
          copy_bytes( uncompressedSize, buffer );
          copy_bytes( compressedSize,   buffer );
          if ( offset > 0 )
            copy_bytes( offset, buffer );
        }
        else if ( offset > 0 )
          copy_bytes( offset, buffer );
      }
    }

    //-------------------------------------------------------------------------
    //! The extra field marker
    //-------------------------------------------------------------------------
    static const uint16_t headerID = 0x0001;

    uint16_t dataSize;         //< size of the extra block
    uint64_t uncompressedSize; //< size of the uncompressed data
    uint64_t compressedSize;   //< size of the compressed data
    uint64_t offset;           //< offset of local header record
    uint32_t nbDisk;           //< number of disk where file starts
    uint16_t totalSize;        //< total size in buffer
  };
}

#endif /* SRC_XRDZIP_XRDZIPEXTRA_HH_ */

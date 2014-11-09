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

#ifndef __XRD_CL_FILE_SYSTEM_UTILS_HH__
#define __XRD_CL_FILE_SYSTEM_UTILS_HH__

#include "XrdCl/XrdClXRootDResponses.hh"

#include <string>
#include <stdint.h>

namespace XrdCl
{
  class FileSystem;

  //----------------------------------------------------------------------------
  //! A container for file system utility functions that do not belong in
  //! FileSystem
  //----------------------------------------------------------------------------
  class FileSystemUtils
  {
    public:
      //------------------------------------------------------------------------
      //! Container for space information
      //------------------------------------------------------------------------
      class SpaceInfo
      {
        public:
          SpaceInfo( uint64_t total, uint64_t free, uint64_t used,
                     uint64_t largestChunk ):
            pTotal( total ), pFree( free ), pUsed( used ),
            pLargestChunk( largestChunk ) { }

          //--------------------------------------------------------------------
          //! Amount of total space in MB
          //--------------------------------------------------------------------
          uint64_t GetTotal() const { return pTotal; }

          //--------------------------------------------------------------------
          //! Amount of free space in MB
          //--------------------------------------------------------------------
          uint64_t GetFree() const { return pFree; }

          //--------------------------------------------------------------------
          //! Amount of used space in MB
          //--------------------------------------------------------------------
          uint64_t GetUsed() const { return pUsed; }

          //--------------------------------------------------------------------
          //! Largest single chunk of free space
          //--------------------------------------------------------------------
          uint64_t GetLargestFreeChunk() const { return pLargestChunk; }

        private:
          uint64_t pTotal;
          uint64_t pFree;
          uint64_t pUsed;
          uint64_t pLargestChunk;
      };

      //------------------------------------------------------------------------
      //! Recursively get space information for given path
      //------------------------------------------------------------------------
      static XRootDStatus GetSpaceInfo( SpaceInfo         *&result,
                                        FileSystem         *fs,
                                        const std::string  &path );
  };
}

#endif // __XRD_CL_FILE_SYSTEM_UTILS HH__

//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>
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

#ifndef __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
#define __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__

#include "XrdCl/XrdClOperations.hh"

namespace XrdCl {

    template <State state>
    class FileSystemOperation: public Operation<state> {
        public:
            //------------------------------------------------------------------
            //! Constructor
            //!
            //! @param fs  filesystem object on which operation will be performed
            //! @param h  operation handler
            //------------------------------------------------------------------
            FileSystemOperation(FileSystem *fs): filesystem(fs){}

        protected:
            //------------------------------------------------------------------
            //! Constructor (used internally to change copy object with 
            //! change of template parameter)
            //!
            //! @param fs  filesystem object on which operation will be performed
            //! @param h  operation handler
            //------------------------------------------------------------------
            FileOperation(FileSystem *fs, std::unique_ptr<OperationHandler> h): Operation<state>(std::move(h)), filesystem(fs){}

            FileSystem *filesystem;

    };
}


#endif // __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <ccaffy@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef XROOTD_XRDUTILS_HH
#define XROOTD_XRDUTILS_HH

#include <string>

namespace XrdUtils {

    class Utils {
    public:
        //------------------------------------------------------------------------
        //! Split a string
        //------------------------------------------------------------------------
        template<class Container>
        static void splitString( Container         &result,
                                 const std::string &input,
                                 const std::string &delimiter )
        {
            size_t start  = 0;
            size_t end    = 0;
            size_t length = 0;

            do
            {
                end = input.find( delimiter, start );

                if( end != std::string::npos )
                    length = end - start;
                else
                    length = input.length() - start;

                if( length )
                    result.push_back( input.substr( start, length ) );

                start = end + delimiter.size();
            }
            while( end != std::string::npos );
        }
    };

}

#endif //XROOTD_XRDUTILS_HH

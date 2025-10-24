/******************************************************************************/
/*                                                                            */
/*                        X r d O u c U t i l s . h h                         */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#ifndef XROOTD_XRDOUCTUTILS_HH
#define XROOTD_XRDOUCTUTILS_HH

#include <string>
#include <map>
#include <algorithm>

/**
 * This class is created to contain template code
 * for utility reason. Its purpose is basically the same as the XrdOucUtils,
 * but it will only contain templated code
 */
class XrdOucTUtils {

public:

//------------------------------------------------------------------------
//! Split a string
//------------------------------------------------------------------------
template<class Container>
static void splitString( Container &result,  std::string_view input, const std::string &delimiter ) {
    size_t start = 0;
    size_t end = 0;
    size_t length = 0;

    do {
        end = input.find(delimiter, start);

        if (end != std::string::npos)
            length = end - start;
        else
            length = input.length() - start;

        if (length)
            result.emplace_back(input.substr(start, length));

        start = end + delimiter.size();
    } while (end != std::string::npos);
}

/**
 * Looks for an element in a std::map<std::string, T> in a case-insensitive way
 * @tparam T the type of the associated element to search
 * @param m the map to search
 * @param lowerCaseSearchKey the key to look for (must be lower-cased!)
 * @return the iterator related to the map find
 */
template<typename T>
static typename std::map<std::string, T>::const_iterator caseInsensitiveFind(const std::map<std::string, T> & m, const std::string & lowerCaseSearchKey) {
  auto it = std::find_if(m.begin(),m.end(), [&lowerCaseSearchKey](const std::pair<std::string, T> & p){
    return std::equal(
      p.first.begin(), p.first.end(),
      lowerCaseSearchKey.begin(), lowerCaseSearchKey.end(),
      [](unsigned char a, unsigned char b) {
        return (std::tolower(a) == b);
      }
    );
  });
  return it;
}

};

#endif //XROOTD_XRDOUCTUTILS_HH

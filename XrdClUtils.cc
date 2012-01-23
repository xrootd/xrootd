//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClUtils.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Split string
  //----------------------------------------------------------------------------
  void Utils::splitString( std::vector<std::string> &result,
                           const std::string  &input,
                           const std::string  &delimiter )
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
        length = std::string::npos;

      result.push_back( input.substr( start, length ) );

      start = end + delimiter.size();
    }
    while( end != std::string::npos );
  }
}

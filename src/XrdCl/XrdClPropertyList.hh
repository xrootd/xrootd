//------------------------------------------------------------------------------
// Copyright (c) 2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//-----------------------------------------------------------------------------
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

#ifndef __XRD_CL_PROPERTY_LIST_HH__
#define __XRD_CL_PROPERTY_LIST_HH__

#include <map>
#include <string>
#include <sstream>
#include <algorithm>

#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! A key-value pair map storing both keys and values as strings
  //----------------------------------------------------------------------------
  class PropertyList
  {
    public:
      typedef std::map<std::string, std::string> PropertyMap;

      //------------------------------------------------------------------------
      //! Associate a value with a key
      //!
      //! @param name  must not contain spaces
      //! @param value needs to be convertible to std::string
      //------------------------------------------------------------------------
      template<typename Item>
      void Set( const std::string &name, const Item &value )
      {
        std::ostringstream o;
        o << value;
        pProperties[name] = o.str();
      }

      //------------------------------------------------------------------------
      //! Get the value associated with a name
      //!
      //! @return true if the name was found, false otherwise
      //------------------------------------------------------------------------
      template<typename Item>
      bool Get( const std::string &name, Item &item ) const
      {
        PropertyMap::const_iterator it;
        it = pProperties.find( name );
        if( it == pProperties.end() )
          return false;
        std::istringstream i; i.str( it->second );
        i >> item;
        if( i.bad() )
          return false;
        return true;
      }

      //------------------------------------------------------------------------
      //! Get the value associated with a name
      //!
      //! @return the value or Item() if the key does not exist
      //------------------------------------------------------------------------
      template<typename Item>
      Item Get( const std::string &name ) const
      {
        PropertyMap::const_iterator it;
        it = pProperties.find( name );
        if( it == pProperties.end() )
          return Item();
        std::istringstream i; i.str( it->second );
        Item item;
        i >> item;
        if( i.bad() )
          return Item();
        return item;
      }

      //------------------------------------------------------------------------
      //! Set a value with a name and an index
      //!
      //! @param name must not contain spaces
      //! @param index
      //! @param value must be convertible to std::string
      //------------------------------------------------------------------------
      template<typename Item>
      void Set( const std::string &name, uint32_t index, const Item &value )
      {
        std::ostringstream o;
        o << name << " " << index;
        Set( o.str(), value );
      }

      //------------------------------------------------------------------------
      //! Get the value associated with a key and an index
      //!
      //! @return true if the key and index were found, false otherwise
      //------------------------------------------------------------------------
      template<typename Item>
      bool Get( const std::string &name, uint32_t index, Item &item ) const
      {
        std::ostringstream o;
        o << name << " " << index;
        return Get( o.str(), item );
      }

      //------------------------------------------------------------------------
      //! Get the value associated with a key and an index
      //!
      //! @return the value or Item() if the key does not exist
      //------------------------------------------------------------------------
      template<typename Item>
      Item Get( const std::string &name, uint32_t index ) const
      {
        std::ostringstream o;
        o << name << " " << index;
        return Get<Item>( o.str() );
      }

      //------------------------------------------------------------------------
      //! Check if we now about the given name
      //------------------------------------------------------------------------
      bool HasProperty( const std::string &name ) const
      {
        return pProperties.find( name ) != pProperties.end();
      }

      //------------------------------------------------------------------------
      //! Check if we know about the given name and index
      //------------------------------------------------------------------------
      bool HasProperty( const std::string &name, uint32_t index ) const
      {
        std::ostringstream o;
        o << name << " " << index;
        return HasProperty( o.str() );
      }

      //------------------------------------------------------------------------
      //! Get the begin iterator
      //------------------------------------------------------------------------
      PropertyMap::const_iterator begin() const
      {
        return pProperties.begin();
      }

      //------------------------------------------------------------------------
      //! Get the end iterator
      //------------------------------------------------------------------------
      PropertyMap::const_iterator end() const
      {
        return pProperties.end();
      }

      //------------------------------------------------------------------------
      //! Clear the property list
      //------------------------------------------------------------------------
      void Clear()
      {
        pProperties.clear();
      }

    private:
      PropertyMap pProperties;
  };

  //----------------------------------------------------------------------------
  // Specialize get for strings
  //----------------------------------------------------------------------------
  template<>
  inline bool PropertyList::Get<std::string>( const std::string &name,
                                              std::string       &item ) const
  {
    PropertyMap::const_iterator it;
    it = pProperties.find( name );
    if( it == pProperties.end() )
      return false;
    item = it->second;
    return true;
  }

  template<>
  inline std::string PropertyList::Get<std::string>( const std::string &name ) const
  {
    PropertyMap::const_iterator it;
    it = pProperties.find( name );
    if( it == pProperties.end() )
      return std::string();
    return it->second;
  }

  //----------------------------------------------------------------------------
  // Specialize set for XRootDStatus
  //----------------------------------------------------------------------------
  template<>
  inline void PropertyList::Set<XRootDStatus>( const std::string  &name,
                                               const XRootDStatus &item )
  {
    std::ostringstream o;
    o << item.status << ";" << item.code << ";" << item.errNo << "#";
    o << item.GetErrorMessage();
    Set( name, o.str() );
  }

  //----------------------------------------------------------------------------
  // Specialize get for XRootDStatus
  //----------------------------------------------------------------------------
  template<>
  inline bool PropertyList::Get<XRootDStatus>( const std::string  &name,
                                               XRootDStatus       &item ) const
  {
    std::string str, msg, tmp;
    if( !Get( name, str ) )
      return false;

    std::string::size_type i;
    i = str.find( '#' );
    if( i == std::string::npos )
      return false;
    item.SetErrorMessage( str.substr( i+1, str.length()-i-1 ) );
    str.erase( i, str.length()-i );
    std::replace( str.begin(), str.end(), ';', ' ' );
    std::istringstream is; is.str( str );
    is >> item.status; if( is.bad() ) return false;
    is >> item.code;   if( is.bad() ) return false;
    is >> item.errNo;  if( is.bad() ) return false;
    return true;
  }

  template<>
  inline XRootDStatus PropertyList::Get<XRootDStatus>(
    const std::string  &name ) const
  {
    XRootDStatus st;
    if( !Get( name, st ) )
      return XRootDStatus();
    return st;
  }

  //----------------------------------------------------------------------------
  // Specialize set for URL
  //----------------------------------------------------------------------------
  template<>
  inline void PropertyList::Set<URL>( const std::string  &name,
                                      const URL          &item )
  {
    Set( name, item.GetURL() );
  }

  //----------------------------------------------------------------------------
  // Specialize get for URL
  //----------------------------------------------------------------------------
  template<>
  inline bool PropertyList::Get<URL>( const std::string  &name,
                                      URL                &item ) const
  {
    std::string tmp;
    if( !Get( name, tmp ) )
      return false;

    item = tmp;
    return true;
  }

  //----------------------------------------------------------------------------
  // Specialize set for vector<string>
  //----------------------------------------------------------------------------
  template<>
  inline void PropertyList::Set<std::vector<std::string> >(
    const std::string              &name,
    const std::vector<std::string> &item )
  {
    std::vector<std::string>::const_iterator it;
    int i = 0;
    for( it = item.begin(); it != item.end(); ++it, ++i )
      Set( name, i, *it );
  }

  //----------------------------------------------------------------------------
  // Specialize get for XRootDStatus
  //----------------------------------------------------------------------------
  template<>
  inline bool PropertyList::Get<std::vector<std::string> >(
    const std::string        &name,
    std::vector<std::string> &item ) const
  {
    std::string tmp;
    item.clear();
    for( int i = 0; HasProperty( name, i ); ++i )
    {
      if( !Get( name, i, tmp ) )
        return false;
      item.push_back( tmp );
    }
    return true;
  }
}

#endif // __XRD_OUC_PROPERTY_LIST_HH__

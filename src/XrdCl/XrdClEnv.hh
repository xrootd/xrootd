//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#ifndef __XRD_CL_ENV_HH__
#define __XRD_CL_ENV_HH__

#include <map>
#include <string>
#include <utility>
#include <algorithm>

#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! A simple key value store intended to hold global configuration.
  //! It is able to import the settings from the shell environment, the
  //! variables imported this way supersede these provided from the C++
  //! code.
  //----------------------------------------------------------------------------
  class Env
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Env() {}

      //------------------------------------------------------------------------
      //! Get a string associated to the given key
      //!
      //! @return true if the value was found, false otherwise
      //------------------------------------------------------------------------
      bool GetString( const std::string &key, std::string &value );

      //------------------------------------------------------------------------
      //! Associate a string with the given key
      //!
      //! @return false if there is already a shell-imported setting for this
      //!         key, true otherwise
      //------------------------------------------------------------------------
      bool PutString( const std::string &key, const std::string &value );

      //------------------------------------------------------------------------
      //! Remove the string associated with the given key
      //!
      //! @return false if there is a shell-imported setting for this key,
      //!         true otherwise
      //------------------------------------------------------------------------
      bool DelString( const std::string &key );

      //------------------------------------------------------------------------
      //! Get an int associated to the given key
      //!
      //! @return true if the value was found, false otherwise
      //------------------------------------------------------------------------
      bool GetInt( const std::string &key, int &value );

      //------------------------------------------------------------------------
      //! Associate an int with the given key
      //!
      //! @return false if there is already a shell-imported setting for this
      //!         key, true otherwise
      //------------------------------------------------------------------------
      bool PutInt( const std::string &key, int value );

      //------------------------------------------------------------------------
      //! Remove the int associated with the given key
      //!
      //! @return false if there is a shell-imported setting for this key,
      //!         true otherwise
      //------------------------------------------------------------------------
      bool DelInt( const std::string &key );

      //------------------------------------------------------------------------
      //! Get a pointer associated to the given key
      //!
      //! @return true if the value was found, false otherwise
      //------------------------------------------------------------------------
      bool GetPtr( const std::string &key, void* &value );

      //------------------------------------------------------------------------
      //! Associate an int with the given key
      //!
      //! @return true if the key was previously unset, false otherwise.
      //------------------------------------------------------------------------
      bool PutPtr( const std::string &key, void* value );

      //------------------------------------------------------------------------
      //! Import an int from the shell environment. Any imported setting
      //! takes precedence over the one set by other means.
      //!
      //! @return true if the setting exists in the shell, false otherwise
      //------------------------------------------------------------------------
      bool ImportInt( const std::string &key, const std::string &shellKey );

      //------------------------------------------------------------------------
      //! Import a string from the shell environment. Any imported setting
      //! takes precedence over the one set by ther means.
      //!
      //! @return true if the setting exists in the shell, false otherwise
      //------------------------------------------------------------------------
      bool ImportString( const std::string &key, const std::string &shellKey );

      //------------------------------------------------------------------------
      //! Get default integer value for the given key
      //! @param key   : the key
      //! @param value : output parameter, default value corresponding to
      //!                the key
      //! @return      : true if a default integer value for the given key
      //!                exists, false otherwise
      //------------------------------------------------------------------------
      bool GetDefaultIntValue( const std::string &key, int &value );

      //------------------------------------------------------------------------
      //! Get default string value for the given key
      //! @param key   : the key
      //! @param value : output parameter, default value corresponding to
      //!                the key
      //! @return      : true if a default string value for the given key
      //!                exists, false otherwise
      //------------------------------------------------------------------------
      bool GetDefaultStringValue( const std::string &key, std::string &value );

      //------------------------------------------------------------------------
      //! Normalize a configuration key for case-insensitive lookup.
      //! The key is lowercased and a leading "xrd_" prefix is removed.
      //! @param key : configuration key to normalize
      //! @return    : normalized key suitable for map lookup
      //------------------------------------------------------------------------
      static std::string UnifyKey( std::string key );

      //------------------------------------------------------------------------
      //! Look up a value in a plug-in configuration map.
      //! Keys are matched case-insensitively after applying UnifyKey().
      //! @param pluginConfig : settings from a client.plugins.d entry
      //! @param key          : configuration key to look up
      //! @return             : value associated with @a key, or an empty
      //!                       string if no matching entry exists
      //------------------------------------------------------------------------
      static std::string GetPluginConfigValue(
          const std::map<std::string, std::string> &pluginConfig,
          const std::string                         &key );

      //------------------------------------------------------------------------
      //! Resolve a string setting using shell environment, then plug-in
      //! configuration, then @a defaultValue.
      //!
      //! The resolved value is stored in the environment under @a key.
      //! @param key          : environment key to store the resolved value
      //!                       under
      //! @param shellKey     : shell environment variable to import from;
      //!                       may be empty to skip shell lookup
      //! @param pluginConfig : optional map of plug-in settings from
      //!                       client.plugins.d; may be null
      //! @param value        : output parameter, receives the resolved
      //!                       value
      //! @param defaultValue : fallback value when neither the shell nor
      //!                       plug-in configuration provides a setting
      //! @return             : true if @a value came from the shell or
      //!                       plug-in configuration, false if
      //!                       @a defaultValue was used
      //------------------------------------------------------------------------
      bool ResolveString( const std::string                         &key,
                          const std::string                         &shellKey,
                          const std::map<std::string, std::string> *pluginConfig,
                          std::string                               &value,
                          const std::string                         &defaultValue = "" );

      //------------------------------------------------------------------------
      //! Resolve an integer setting using shell environment, then plug-in
      //! configuration, then @a defaultValue.
      //!
      //! The resolved value is stored in the environment under @a key.
      //! @param key          : environment key to store the resolved value
      //!                       under
      //! @param shellKey     : shell environment variable to import from;
      //!                       may be empty to skip shell lookup
      //! @param pluginConfig : optional map of plug-in settings from
      //!                       client.plugins.d; may be null
      //! @param value        : output parameter, receives the resolved
      //!                       value
      //! @param defaultValue : fallback value when neither the shell nor
      //!                       plug-in configuration provides a setting
      //! @return             : true if @a value came from the shell or
      //!                       plug-in configuration, false if
      //!                       @a defaultValue was used
      //------------------------------------------------------------------------
      bool ResolveInt( const std::string                         &key,
                       const std::string                         &shellKey,
                       const std::map<std::string, std::string> *pluginConfig,
                       int                                       &value,
                       int                                        defaultValue = 0 );

      //------------------------------------------------------------------------
      // Lock the environment for writing
      //------------------------------------------------------------------------
      void WriteLock()
      {
        pLock.WriteLock();
      }

      //------------------------------------------------------------------------
      // Unlock the environment
      //------------------------------------------------------------------------
      void UnLock()
      {
        pLock.UnLock();
      }

      //------------------------------------------------------------------------
      // Re-initialize the lock
      //------------------------------------------------------------------------
      void ReInitializeLock()
      {
        // this is really shaky, but seems to work on linux and fork safety
        // is probably not required anywhere else
        pLock.UnLock();
        pLock.ReInitialize();
      }

      //------------------------------------------------------------------------
      // Re-create the lock in the same memory
      //------------------------------------------------------------------------
      void RecreateLock()
      {
        new( &pLock )XrdSysRWLock();
      }

    private:

      std::string GetEnv( const std::string &key );
      typedef std::map<std::string, std::pair<std::string, bool> > StringMap;
      typedef std::map<std::string, std::pair<int, bool> >         IntMap;
      typedef std::map<std::string, void* >                        PtrMap;

      XrdSysRWLock pLock;
      StringMap    pStringMap;
      IntMap       pIntMap;
      PtrMap       pPtrMap;
  };
}

#endif // __XRD_CL_ENV_HH__

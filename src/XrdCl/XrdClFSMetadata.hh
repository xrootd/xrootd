#ifndef __XRD_CL_FS_METADATA_HH__
#define __XRD_CL_FS_METADATA_HH__

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <iostream>

namespace XrdCl
{
  namespace FSMetadata
  {
    inline std::string PathOnly( const std::string &path, bool local )
    {
      return local ? path : path.substr( 0, path.find( '?' ) );
    }

    inline bool IsJSONText( const std::string &value )
    {
      try
      {
        static_cast<void>( nlohmann::json( value ).dump() );
        return true;
      }
      catch( const nlohmann::json::type_error & )
      {
        return false;
      }
    }

    inline nlohmann::json Entry( const std::string &path, const StatInfo &info )
    {
      const bool extended = info.ExtendedFormat();
      const char *type = info.TestFlags( StatInfo::IsDir ) ? "directory" :
                         info.TestFlags( StatInfo::Other ) ? "other" : "file";
      nlohmann::json names = nlohmann::json::array();
      const std::pair<StatInfo::Flags, const char *> flags[] = {
        { StatInfo::XBitSet, "XBitSet" },
        { StatInfo::IsDir, "IsDir" },
        { StatInfo::Other, "Other" },
        { StatInfo::Offline, "Offline" },
        { StatInfo::POSCPending, "POSCPending" },
        { StatInfo::IsReadable, "IsReadable" },
        { StatInfo::IsWritable, "IsWritable" },
        { StatInfo::BackUpExists, "BackUpExists" }
      };
      for( const auto &flag : flags )
        if( info.TestFlags( flag.first ) ) names.push_back( flag.second );

      auto optional = []( bool present, const std::string &value ) -> nlohmann::json
      {
        if( present && IsJSONText( value ) ) return value;
        return nullptr;
      };
      // The legacy StatInfo getters expose epoch seconds as unsigned integers.
      return {
        { "path", path },
        { "type", type },
        { "size", info.GetSize() },
        { "mtime", static_cast<int64_t>( info.GetModTime() ) },
        { "atime", extended ? nlohmann::json( static_cast<int64_t>( info.GetAccessTime() ) ) : nullptr },
        { "ctime", extended ? nlohmann::json( static_cast<int64_t>( info.GetChangeTime() ) ) : nullptr },
        { "flags", info.GetFlags() },
        { "flag_names", std::move( names ) },
        { "extended", extended },
        { "mode", optional( extended, info.GetModeAsString() ) },
        { "permissions", optional( extended, info.GetModeAsOctString() ) },
        { "owner", optional( extended, info.GetOwner() ) },
        { "group", optional( extended, info.GetGroup() ) },
        { "checksum", optional( info.HasChecksum(), info.GetChecksum() ) },
        { "xattrs", nlohmann::json::array() }
      };
    }

    inline bool AppendEntry( nlohmann::json &entries, const std::string &path,
                             const StatInfo &info )
    {
      // Replacing bytes in a path would give the entry a different name.
      if( !IsJSONText( path ) ) return false;
      entries.push_back( Entry( path, info ) );
      return true;
    }

    inline XRootDStatus Print( const XRootDStatus &status,
                               nlohmann::json entries = nlohmann::json::array() )
    {
      const nlohmann::json result = {
        { "version", 1 },
        { "status", {
            { "ok", status.IsOK() },
            { "code", status.code },
            { "number", status.errNo },
            { "partial", status.IsOK() && status.code == suPartial }
        } },
        { "entries", std::move( entries ) }
      };
      std::cout << result.dump() << std::endl;
      return std::cout ? status : XRootDStatus( stError, errOSError );
    }

    inline XRootDStatus PrintEntry( const XRootDStatus &status,
                                    const std::string &path,
                                    const StatInfo &info )
    {
      auto entries = nlohmann::json::array();
      if( !AppendEntry( entries, path, info ) )
        return Print( XRootDStatus( stError, errDataError ) );
      return Print( status, std::move( entries ) );
    }
  }
}

#endif

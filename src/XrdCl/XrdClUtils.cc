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

#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClOptimizers.hh"
#include "XrdNet/XrdNetAddr.hh"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <functional>
#include <cctype>
#include <locale>
#include <map>
#include <string>
#include <set>
#include <cctype>
#include <random>
#include <chrono>

#include <sys/types.h>
#include <dirent.h>

#if __cplusplus < 201103L
#include <ctime>
#endif

namespace
{
  bool isNotSpace( char c )
  {
    return c != ' ';
  }

  //----------------------------------------------------------------------------
  // Ordering function for sorting IP addresses
  //----------------------------------------------------------------------------
  struct PreferIPv6
  {
    bool operator() ( const XrdNetAddr &l, const XrdNetAddr &r )
    {
      bool rIsIPv4 = false;
      if( r.isIPType( XrdNetAddrInfo::IPv4 ) ||
          (r.isIPType( XrdNetAddrInfo::IPv6 ) && r.isMapped()) )
        rIsIPv4 = true;

      if( l.isIPType( XrdNetAddrInfo::IPv6 ) && rIsIPv4 )
        return true;
      return false;
    }
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Get a parameter either from the environment or URL
  //----------------------------------------------------------------------------
  int Utils::GetIntParameter( const URL         &url,
                              const std::string &name,
                              int                defaultVal )
  {
    Env                            *env = DefaultEnv::GetEnv();
    int                             value = defaultVal;
    char                           *endPtr;
    URL::ParamsMap::const_iterator  it;

    env->GetInt( name, value );
    it = url.GetParams().find( std::string("XrdCl.") + name );
    if( it != url.GetParams().end() )
    {
      int urlValue = (int)strtol( it->second.c_str(), &endPtr, 0 );
      if( !*endPtr )
        value = urlValue;
    }
    return value;
  }

  //----------------------------------------------------------------------------
  // Get a parameter either from the environment or URL
  //----------------------------------------------------------------------------
  std::string Utils::GetStringParameter( const URL         &url,
                                         const std::string &name,
                                         const std::string &defaultVal )
  {
    Env                            *env = DefaultEnv::GetEnv();
    std::string                     value = defaultVal;
    URL::ParamsMap::const_iterator  it;

    env->GetString( name, value );
    it = url.GetParams().find( std::string("XrdCl.") + name );
    if( it != url.GetParams().end() )
      value = it->second;

    return value;
  }

  //----------------------------------------------------------------------------
  // Interpret a string as address type, default to IPAll
  //----------------------------------------------------------------------------
  Utils::AddressType Utils::String2AddressType( const std::string &addressType )
  {
    if( addressType == "IPv6" )
      return IPv6;
    else if( addressType == "IPv4" )
      return IPv4;
    else if( addressType == "IPv4Mapped6" )
      return IPv4Mapped6;
    else if( addressType == "IPAll" )
      return IPAll;
    else
      return IPAuto;
  }

  //----------------------------------------------------------------------------
  // Resolve IP addresses
  //----------------------------------------------------------------------------
  Status Utils::GetHostAddresses( std::vector<XrdNetAddr> &addresses,
                                  const URL               &url,
                                  Utils::AddressType      type )
  {
    Log *log = DefaultEnv::GetLog();
    const char *err    = 0;
    int ordn;

    //--------------------------------------------------------------------------
    // Resolve all the addresses
    //--------------------------------------------------------------------------
    std::ostringstream o; o << url.GetHostName() << ":" << url.GetPort();
    XrdNetUtils::AddrOpts opts;

    if( type == IPv6 ) opts = XrdNetUtils::onlyIPv6;
    else if( type == IPv4 ) opts = XrdNetUtils::onlyIPv4;
    else if( type == IPv4Mapped6 ) opts = XrdNetUtils::allV4Map;
    else if( type == IPAll ) opts = XrdNetUtils::allIPMap;
    else opts = XrdNetUtils::prefAuto;

    //--------------------------------------------------------------------------
    // Check what are the preferences IPv6 or IPv4
    //--------------------------------------------------------------------------
    int preferIPv4 = DefaultPreferIPv4;
    DefaultEnv::GetEnv()->GetInt( "PreferIPv4", preferIPv4 );

    //--------------------------------------------------------------------------
    // Partition the addresses according to the preferences
    //
    // The preferred IP family goes to the back as it is easier to remove
    // items from the back of the vector
    //--------------------------------------------------------------------------
    opts |= (preferIPv4 ? XrdNetUtils::order64 : XrdNetUtils::order46);

    //--------------------------------------------------------------------------
    // Now get all of the properly partitioned addresses; ordn will hold the
    // number of non-preferred addresses at the front of the table.
    //--------------------------------------------------------------------------
    err = XrdNetUtils::GetAddrs( o.str(), addresses, &ordn, opts );

    if( err )
    {
      log->Error( UtilityMsg, "Unable to resolve %s: %s", o.str().c_str(),
                  err );
      return Status( stError, errInvalidAddr );
    }

    if( addresses.size() == 0 )
    {
      log->Error( UtilityMsg, "No addresses for %s were found",
                  o.str().c_str() );
      return Status( stError, errInvalidAddr );
    }

    //--------------------------------------------------------------------------
    // Shuffle each partition
    //--------------------------------------------------------------------------

    int ipNoShuffle = DefaultIPNoShuffle;
    Env *env = DefaultEnv::GetEnv();
    env->GetInt( "IPNoShuffle", ipNoShuffle );

    if( !ipNoShuffle )
    {
#if __cplusplus < 201103L
      // initialize the random generator only once
      static struct only_once_t
      {
        only_once_t()
        {
          std::srand ( unsigned ( std::time(0) ) );
        }
      } only_once;

      std::random_shuffle( addresses.begin(), addresses.begin() + ordn );
      std::random_shuffle( addresses.begin() + ordn, addresses.end() );
#else
      static std::default_random_engine rand_engine(
          std::chrono::system_clock::now().time_since_epoch().count() );

      std::shuffle( addresses.begin(), addresses.begin() + ordn, rand_engine );
      std::shuffle( addresses.begin() + ordn, addresses.end(), rand_engine );
#endif
    }

    //--------------------------------------------------------------------------
    // Return status as the result is already in the output parameter
    //--------------------------------------------------------------------------
    return Status();
  }

  //----------------------------------------------------------------------------
  // Log all the addresses on the list
  //----------------------------------------------------------------------------
  void Utils::LogHostAddresses( Log                     *log,
                                uint64_t                 type,
                                const std::string       &hostId,
                                std::vector<XrdNetAddr> &addresses )
  {
    std::string addrStr;
    std::vector<XrdNetAddr>::iterator it;
    for( it = addresses.begin(); it != addresses.end(); ++it )
    {
      char nameBuff[256];
      it->Format( nameBuff, 256, XrdNetAddrInfo::fmtAdv6 );
      addrStr += nameBuff;
      addrStr += ", ";
    }
    addrStr.erase( addrStr.length()-2, 2 );
    log->Debug( type, "[%s] Found %zu address(es): %s",
                      hostId.c_str(), addresses.size(), addrStr.c_str() );
  }

  //----------------------------------------------------------------------------
  // Convert timestamp to a string
  //----------------------------------------------------------------------------
  std::string Utils::TimeToString( time_t timestamp )
  {
    char   now[30];
    tm     tsNow;
    time_t ttNow = timestamp;
    localtime_r( &ttNow, &tsNow );
    strftime( now, 30, "%Y-%m-%d %H:%M:%S %z", &tsNow );
    return now;
  }

  //----------------------------------------------------------------------------
  // Get the elapsed microseconds between two timevals
  //----------------------------------------------------------------------------
  uint64_t Utils::GetElapsedMicroSecs( timeval start, timeval end )
  {
    uint64_t startUSec = start.tv_sec*1000000 + start.tv_usec;
    uint64_t endUSec   = end.tv_sec*1000000 + end.tv_usec;
    return endUSec-startUSec;
  }

  //----------------------------------------------------------------------------
  // Get remote checksum
  //----------------------------------------------------------------------------
  XRootDStatus Utils::GetRemoteCheckSum( std::string       &checkSum,
                                         const std::string &checkSumType,
                                         const URL         &url )
  {
    FileSystem   *fs = new FileSystem( url );
    // add the 'cks.type' cgi tag in order to
    // select the proper checksum type in case
    // the server supports more than one checksum
    size_t pos = url.GetPath().find( '?' );
    std::string cksPath = url.GetPath() + ( pos == std::string::npos ? '?' : '&' ) + "cks.type=" + checkSumType;
    Buffer        arg; arg.FromString( cksPath );
    Buffer       *cksResponse = 0;
    XRootDStatus  st;
    Log          *log    = DefaultEnv::GetLog();

    st = fs->Query( QueryCode::Checksum, arg, cksResponse );
    delete fs;

    if( !st.IsOK() )
    {
      std::string msg = st.GetErrorMessage();
      msg += " Got an error while querying the checksum!";
      st.SetErrorMessage( msg );
      return st;
    }

    if( !cksResponse )
      return XRootDStatus( stError, errInternal, 0, "Got invalid response while querying the checksum!" );

    std::vector<std::string> elems;
    Utils::splitString( elems, cksResponse->ToString(), " " );
    delete cksResponse;

    if( elems.size() != 2 )
      return XRootDStatus( stError, errInvalidResponse, 0, "Got invalid response while querying the checksum!" );

    if( elems[0] != checkSumType )
      return XRootDStatus( stError, errCheckSumError );

    checkSum = elems[0] + ":";
    checkSum += NormalizeChecksum( elems[0], elems[1] );

    log->Dump( UtilityMsg, "Checksum for %s checksum: %s",
               url.GetPath().c_str(), checkSum.c_str() );

    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Get a checksum from local file
  //------------------------------------------------------------------------
  XRootDStatus Utils::GetLocalCheckSum( std::string       &checkSum,
                                        const std::string &checkSumType,
                                        const std::string &path )
  {
    Log             *log    = DefaultEnv::GetLog();
    CheckSumManager *cksMan = DefaultEnv::GetCheckSumManager();

    if( !cksMan )
    {
      log->Error( UtilityMsg, "Unable to get the checksum manager" );
      return XRootDStatus( stError, errInternal );
    }

    XrdCksData ckSum; ckSum.Set( checkSumType.c_str() );
    bool status = cksMan->Calculate( ckSum, checkSumType, path.c_str() );
    if( !status )
    {
      log->Error( UtilityMsg, "Error while calculating checksum for %s",
                  path.c_str() );
      return XRootDStatus( stError, errCheckSumError );
    }

    char *cksBuffer = new char[265];
    ckSum.Get( cksBuffer, 256 );
    checkSum  = checkSumType + ":";
    checkSum += NormalizeChecksum( checkSumType, cksBuffer );
    delete [] cksBuffer;

    log->Dump( UtilityMsg, "Checksum for %s is: %s", path.c_str(),
               checkSum.c_str() );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Convert bytes to a human readable string
  //----------------------------------------------------------------------------
  std::string Utils::BytesToString( uint64_t bytes )
  {
    double  final  = bytes;
    int     i      = 0;
    char    suf[3] = { 'k', 'M', 'G' };
    for( i = 0; i < 3 && final > 1024; ++i, final /= 1024 ) {};
    std::ostringstream o;
    o << std::setprecision(4) << final;
    if( i > 0 ) o << suf[i-1];
    return o.str();
  }

  //----------------------------------------------------------------------------
  // Check if peer supports tpc
  //----------------------------------------------------------------------------
  XRootDStatus Utils::CheckTPC( const std::string &server, time_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Checking if the data server %s supports tpc",
                server.c_str() );

    FileSystem    sourceDSFS( server );
    Buffer        queryArg; queryArg.FromString( "tpc" );
    Buffer       *queryResponse = 0;
    XRootDStatus  st;
    st = sourceDSFS.Query( QueryCode::Config, queryArg, queryResponse,
                           timeout );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Cannot query source data server %s: %s",
                  server.c_str(), st.ToStr().c_str() );
      st.status = stFatal;
      return st;
    }

    if( !queryResponse )
    {
      log->Error( UtilityMsg, "Cannot query source data server: empty response." );
      st.status = stFatal;
      return st;
    }

    std::string answer = queryResponse->ToString();
    delete queryResponse;
    if( answer.length() == 1 || !isdigit( answer[0] ) || atoi(answer.c_str()) == 0)
    {
      log->Debug( UtilityMsg, "Third party copy not supported at: %s",
                  server.c_str() );
      return XRootDStatus( stError, errNotSupported );
    }
    log->Debug( UtilityMsg, "Third party copy supported at: %s",
                server.c_str() );

    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Check if peer supports tpc / tpc lite
  //------------------------------------------------------------------------
  XRootDStatus Utils::CheckTPCLite( const std::string &server, time_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Checking if the data server %s supports tpc / tpc lite",
                server.c_str() );

    FileSystem    sourceDSFS( server );
    Buffer        queryArg; queryArg.FromString( "tpc tpcdlg" );
    Buffer       *queryResponse = 0;
    XRootDStatus  st;
    st = sourceDSFS.Query( QueryCode::Config, queryArg, queryResponse,
                           timeout );
    if( !st.IsOK() )
    {
      log->Error( UtilityMsg, "Cannot query source data server %s: %s",
                  server.c_str(), st.ToStr().c_str() );
      st.status = stFatal;
      return st;
    }

    if( !queryResponse )
    {
      log->Error( UtilityMsg, "Cannot query source data server: empty response." );
      st.status = stFatal;
      return st;
    }

    std::string answer = queryResponse->ToString();
    delete queryResponse;

    if( answer.empty() )
    {
      log->Error( UtilityMsg, "Cannot query source data server: empty response." );
      st.status = stFatal;
      return st;
    }

    std::vector<std::string> resp;
    Utils::splitString( resp, answer, "\n" );

    if( resp.empty() || resp[0].empty() ||
        !isdigit( resp[0][0]) || atoi( resp[0].c_str() ) == 0 )
    {
      log->Debug( UtilityMsg, "Third party copy not supported at: %s",
                  server.c_str() );
      return XRootDStatus( stError, errNotSupported );
    }

    if( resp.size() == 1 || resp[1] == "tpcdlg" )
    {
      log->Debug( UtilityMsg, "TPC lite not supported at: %s",
                  server.c_str() );
      return XRootDStatus( stOK, suPartial );
    }

    log->Debug( UtilityMsg, "TPC lite supported at: %s",
                server.c_str() );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Convert the fully qualified host name to country code
  //----------------------------------------------------------------------------
  std::string Utils::FQDNToCC( const std::string &fqdn )
  {
    std::vector<std::string> el;
    Utils::splitString( el, fqdn, "." );
    if( el.size() < 2 )
      return "us";

    std::string cc = *el.rbegin();
    if( cc.length() == 2 )
      return cc;
    return "us";
  }

  //----------------------------------------------------------------------------
  // Get directory entries
  //----------------------------------------------------------------------------
  Status Utils::GetDirectoryEntries( std::vector<std::string> &entries,
                                     const std::string        &path )
  {
    DIR *dp = opendir( path.c_str() );
    if( !dp )
      return Status( stError, errOSError, errno );

    dirent *dirEntry;

    while( (dirEntry = readdir(dp)) != 0 )
    {
      std::string entryName = dirEntry->d_name;
      if( !entryName.compare( 0, 2, "..") )
        continue;
      if( !entryName.compare( 0, 1, ".") )
        continue;

      entries.push_back( dirEntry->d_name );
    }

    closedir(dp);

    return Status();
  }

  //----------------------------------------------------------------------------
  // Process a config file and return key-value pairs
  //----------------------------------------------------------------------------
  Status Utils::ProcessConfig( std::map<std::string, std::string> &config,
                               const std::string                  &file )
  {
    config.clear();
    std::ifstream inFile( file.c_str() );
    if( !inFile.good() )
      return Status( stError, errOSError, errno );

    errno = 0;
    std::string line;
    while( std::getline( inFile, line ) )
    {
      if( line.empty() || line[0] == '#' )
        continue;

      std::vector<std::string> elems;
      splitString( elems, line, "=" );
      if( elems.size() != 2 )
        return Status( stError, errConfig );
      std::string key   = elems[0]; Trim( key );
      std::string value = elems[1]; Trim( value );
      config[key] = value;
    }

    if( errno )
      return Status( stError, errOSError, errno );
    return Status();
  }

  //------------------------------------------------------------------------
  //! Process a config directory and return key-value pairs
  //------------------------------------------------------------------------
  Status Utils::ProcessConfigDir( std::map<std::string, std::string> &config,
                                  const std::string                  &dir )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Processing configuration files in %s...",
                dir.c_str());

    std::vector<std::string> entries;
    Status st = Utils::GetDirectoryEntries( entries, dir );
    if( !st.IsOK() )
    {
      log->Debug( UtilityMsg, "Unable to process directory %s: %s",
                  dir.c_str(), st.ToString().c_str() );
      return st;
    }

    static const std::string suffix   = ".conf";
    for( auto &entry : entries )
    {
      std::string confFile = dir + "/" + entry;

      if( confFile.length() <= suffix.length() )
        continue;
      if( !std::equal( suffix.rbegin(), suffix.rend(), confFile.rbegin() ) )
        continue;

      st = ProcessConfig( config, confFile );
      if( !st.IsOK() )
      {
        log->Debug( UtilityMsg, "Unable to process configuration file %s: %s",
                    confFile.c_str(), st.ToString().c_str() );
      }
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // Trim a string
  //----------------------------------------------------------------------------
  void Utils::Trim( std::string &str )
  {
    str.erase( str.begin(),
               std::find_if( str.begin(), str.end(), isNotSpace ) );
    str.erase( std::find_if( str.rbegin(), str.rend(), isNotSpace ).base(),
               str.end() );
  }

  //----------------------------------------------------------------------------
  // Log property list
  //----------------------------------------------------------------------------
  void Utils::LogPropertyList( Log                *log,
                               uint64_t            topic,
                               const char         *format,
                               const PropertyList &list )
  {
    if( unlikely(log->GetLevel() >= Log::DumpMsg) ) {
      PropertyList::PropertyMap::const_iterator it;
      std::string keyVals;
      for (it = list.begin(); it != list.end(); ++it)
        keyVals += "'" + it->first + "' = '" + obfuscateAuth(it->second) + "', ";
      keyVals.erase(keyVals.length() - 2, 2);
      log->Dump(topic, format, keyVals.c_str());
    }
  }

  //----------------------------------------------------------------------------
  // Print a char array as hex
  //----------------------------------------------------------------------------
  std::string Utils::Char2Hex( uint8_t *array, uint16_t size )
  {
    char *hex = new char[2*size+1];
    for( uint16_t i = 0; i < size; ++i )
      snprintf( hex+(2*i), 3, "%02x", (int)array[i] );
    std::string result = hex;
    delete [] hex;
    return result;
  }

  //----------------------------------------------------------------------------
  // Normalize checksum
  //----------------------------------------------------------------------------
  std::string Utils::NormalizeChecksum( const std::string &name,
                                        const std::string &checksum )
  {
    if( name == "adler32" || name == "crc32" )
    {
      size_t i;
      for( i = 0; i < checksum.length(); ++i )
        if( checksum[i] != '0' )
          break;
      return checksum.substr(i);
    }
    return checksum;
  }

  //----------------------------------------------------------------------------
  // Get supported checksum types for given URL
  //----------------------------------------------------------------------------
  std::vector<std::string> Utils::GetSupportedCheckSums( const XrdCl::URL &url )
  {
    std::vector<std::string> ret;

    FileSystem fs( url );
    Buffer  arg; arg.FromString( "chksum" );
    Buffer *resp = 0;
    XRootDStatus st = fs.Query( QueryCode::Config, arg, resp );
    if( st.IsOK() )
    {
      std::string response = resp->ToString();
      if( response != "chksum" )
      {
        // we are expecting a response of format: '0:zcrc32,1:adler32'
        std::vector<std::string> result;
        Utils::splitString( result, response, "," );

        std::vector<std::string>::iterator itr = result.begin();
        for( ; itr != result.end(); ++itr )
        {
          size_t pos = itr->find( ':' );
          if( pos == std::string::npos ) continue;
          std::string cksname = itr->substr( pos + 1 );
          // remove all white spaces
          cksname.erase( std::remove_if( cksname.begin(), cksname.end(), ::isspace ), 
                         cksname.end() );
          ret.push_back( std::move( cksname ) );
        }
      }
    }

    return ret;
  }


  //------------------------------------------------------------------------
  //! Check if this client can support given EC redirect
  //------------------------------------------------------------------------
  bool Utils::CheckEC( const Message *req, const URL &url )
  {
#ifdef WITH_XRDEC
    // make sure that if we will be writing it is a new file
    ClientRequest *request = (ClientRequest*)req->GetBuffer();
    uint16_t options = ntohs( request->open.options );
    bool open_wrt = ( options & kXR_open_updt ) || ( options & kXR_open_wrto );
    bool open_new = ( options & kXR_new );
    if( open_wrt && !open_new ) return false;

    const URL::ParamsMap &params = url.GetParams();
    // make sure all the xrdec. tokens are present and the values are sane
    URL::ParamsMap::const_iterator itr = params.find( "xrdec.nbdta" );
    if( itr == params.end() ) return false;
    size_t nbdta = std::stoul( itr->second );

    itr = params.find( "xrdec.nbprt" );
    if( itr == params.end() ) return false;
    size_t nbprt = std::stoul( itr->second );

    itr = params.find( "xrdec.blksz" );
    if( itr == params.end() ) return false;

    itr = params.find( "xrdec.plgr" );
    if( itr == params.end() ) return false;
    std::vector<std::string> plgr;
    splitString( plgr, itr->second, "," );
    if( plgr.size() < nbdta + nbprt ) return false;

    itr = params.find( "xrdec.objid" );
    if( itr == params.end() ) return false;

    itr = params.find( "xrdec.format" );
    if( itr == params.end() ) return false;
    size_t format = std::stoul( itr->second );
    if( format != 1 ) return false; // TODO use constant

    itr = params.find( "xrdec.dtacgi" );
    if( itr != params.end() )
    {
      std::vector<std::string> dtacgi;
      splitString( dtacgi, itr->second, "," );
      if( plgr.size() != dtacgi.size() ) return false;
    }

    itr = params.find( "xrdec.mdtacgi" );
    if( itr != params.end() )
    {
      std::vector<std::string> mdtacgi;
      splitString( mdtacgi, itr->second, "," );
      if( plgr.size() != mdtacgi.size() ) return false;
    }

    itr = params.find( "xrdec.cosc" );
    if( itr == params.end() ) return false;
    std::string cosc = itr->second;
    if( cosc != "true" && cosc != "false" ) return false;

    return true;
#else
    return false;
#endif
  }


  //----------------------------------------------------------------------------
  //! Automatically infer the right checksum type
  //----------------------------------------------------------------------------
  std::string Utils::InferChecksumType( const XrdCl::URL &source,
                                        const XrdCl::URL &destination,
                                        bool              zip)
  {
    //--------------------------------------------------------------------------
    // If both files are local we won't be checksumming at all
    //--------------------------------------------------------------------------
    if( source.IsLocalFile() && !source.IsMetalink() && destination.IsLocalFile() ) return std::string();

    // checksums supported by local files
    std::set<std::string> local_supported;
    local_supported.insert( "adler32" );
    local_supported.insert( "crc32" );
    local_supported.insert( "md5" );
    local_supported.insert( "zcrc32" );

    std::vector<std::string> srccks;

    if( source.IsMetalink() )
    {
      int useMtlnCksum = DefaultZipMtlnCksum;
      Env *env = DefaultEnv::GetEnv();
      env->GetInt( "ZipMtlnCksum", useMtlnCksum );

      //------------------------------------------------------------------------
      // In case of ZIP use other checksums than zcrc32 only if the user
      // requested it explicitly.
      //------------------------------------------------------------------------
      if( !zip || ( zip && useMtlnCksum ) )
      {
        RedirectorRegistry &registry   = RedirectorRegistry::Instance();
        VirtualRedirector  *redirector = registry.Get( source );
        std::vector<std::string> cks = redirector->GetSupportedCheckSums();
        srccks.insert( srccks.end(), cks.begin(), cks.end() );
      }
    }

    if( zip )
    {
      //------------------------------------------------------------------------
      // In case of ZIP we can always extract the checksum from the archive
      //------------------------------------------------------------------------
      srccks.push_back( "zcrc32" );
    }
    else if( source.GetProtocol() == "root" || source.GetProtocol() == "xroot" )
    {
      //------------------------------------------------------------------------
      // If the source is a remote endpoint query the supported checksums
      //------------------------------------------------------------------------
      std::vector<std::string> cks = GetSupportedCheckSums( source );
      srccks.insert( srccks.end(), cks.begin(), cks.end() );
    }

    std::vector<std::string> dstcks;

    if( destination.GetProtocol() == "root" ||
        destination.GetProtocol() == "xroot" )
    {
      //------------------------------------------------------------------------
      // If the destination is a remote endpoint query the supported checksums
      //------------------------------------------------------------------------
      std::vector<std::string> cks = GetSupportedCheckSums( destination );
      dstcks.insert( dstcks.end(), cks.begin(), cks.end() );
    }

    //--------------------------------------------------------------------------
    // Now we have all the information we need, we can infer the right checksum
    // type!!!
    //
    // First check if source is local
    //--------------------------------------------------------------------------
    if( source.IsLocalFile() && !source.IsMetalink() )
    {
      std::vector<std::string>::iterator itr = dstcks.begin();
      for( ; itr != dstcks.end(); ++itr )
        if( local_supported.count( *itr ) ) return *itr;
      return std::string();
    }

    //--------------------------------------------------------------------------
    // then check if destination is local
    //--------------------------------------------------------------------------
    if( destination.IsLocalFile() )
    {
      std::vector<std::string>::iterator itr = srccks.begin();
      for( ; itr != srccks.end(); ++itr )
        if( local_supported.count( *itr ) ) return *itr;
      return std::string();
    }

    //--------------------------------------------------------------------------
    // if both source and destination are remote look for a checksum that can
    // satisfy both
    //--------------------------------------------------------------------------
    std::set<std::string> dst_supported( dstcks.begin(), dstcks.end() );
    std::vector<std::string>::iterator itr = srccks.begin();
    for( ; itr != srccks.end(); ++itr )
      if( dst_supported.count( *itr ) ) return *itr;
    return std::string();
  }

  //----------------------------------------------------------------------------
  //! Split chunks in a ChunkList into one or more ChunkLists
  //----------------------------------------------------------------------------
  void Utils::SplitChunks( std::vector<ChunkList> &listsvec,
                           const ChunkList        &chunks,
                           const uint32_t          maxcs,
                           const size_t            maxc )
  {
    listsvec.clear();
    if( !chunks.size() ) return;

    listsvec.emplace_back();
    ChunkList *c    = &listsvec.back();
    const size_t cs = chunks.size();
    size_t idx      = 0;
    size_t nc       = 0;
    ChunkInfo tmpc;

    c->reserve( cs );

    while( idx < cs )
    {
      if( maxc && nc >= maxc )
      {
        listsvec.emplace_back();
        c = &listsvec.back();
        c->reserve( cs - idx );
        nc = 0;
      }

      if( tmpc.length == 0 )
        tmpc = chunks[idx];

      if( maxcs && tmpc.length > maxcs )
      {
        c->emplace_back( tmpc.offset, maxcs, tmpc.buffer );
        tmpc.offset += maxcs;
        tmpc.length -= maxcs;
        tmpc.buffer  = static_cast<char*>( tmpc.buffer ) + maxcs;
      }
      else
      {
        c->emplace_back( tmpc.offset, tmpc.length, tmpc.buffer );
        tmpc.length = 0;
        ++idx;
      }
      ++nc;
    }
  }
}

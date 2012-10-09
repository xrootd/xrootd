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
#include "XrdSys/XrdSysDNS.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdCks/XrdCksCalc.hh"

#include <algorithm>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Resolve IP addresses
  //----------------------------------------------------------------------------
  Status Utils::GetHostAddresses( std::vector<sockaddr_in> &addresses,
                                  const URL                &url )
  {
    //--------------------------------------------------------------------------
    // The address resolution algorithm in XRootD has a weird interface
    // so we need to limit the number of possible IP addresses for
    // a given hostname to 25. Why? Because.
    //--------------------------------------------------------------------------
    sockaddr_in *sa = (sockaddr_in*)malloc( sizeof(sockaddr_in) * 25 );
    int numAddr = XrdSysDNS::getHostAddr( url.GetHostName().c_str(),
                                          (sockaddr*)sa, 25 );
    if( numAddr == 0 )
    {
      free( sa );
      return Status( stError, errInvalidAddr );
    }

    addresses.resize( numAddr );
    std::vector<sockaddr_in>::iterator it = addresses.begin();;
    for( int i = 0; i < numAddr; ++i, ++it )
    {
      memcpy( &(*it), &sa[i], sizeof( sockaddr_in ) );
      it->sin_port = htons( (unsigned short) url.GetPort() );
    }
    free( sa );

    std::random_shuffle( addresses.begin(), addresses.end() );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Log all the addresses on the list
  //----------------------------------------------------------------------------
  void Utils::LogHostAddresses( Log                      *log,
                                uint64_t                  type,
                                const std::string        &hostId,
                                std::vector<sockaddr_in> &addresses )
  {
    std::string addrStr;
    std::vector<sockaddr_in>::iterator it;
    for( it = addresses.begin(); it != addresses.end(); ++it )
    {
      char nameBuff[256];
      XrdSysDNS::IPFormat( (sockaddr*)&(*it), nameBuff, sizeof(nameBuff) );
      addrStr += nameBuff;
      addrStr += ", ";
    }
    addrStr.erase( addrStr.length()-2, 2 );
    log->Debug( type, "[%s] Found %d address(es): %s",
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
  // Get the elapsed mictoseconds between two timevals
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
                                         const std::string &server,
                                         const std::string &path )
  {
    FileSystem   *fs = new FileSystem( URL( server ) );
    Buffer        arg; arg.FromString( path );
    Buffer       *cksResponse = 0;
    XRootDStatus  st;
    Log          *log    = DefaultEnv::GetLog();

    st = fs->Query( QueryCode::Checksum, arg, cksResponse );
    delete fs;

    if( !st.IsOK() )
      return st;

    if( !cksResponse )
      return XRootDStatus( stError, errInternal );

    std::vector<std::string> elems;
    Utils::splitString( elems, cksResponse->ToString(), " " );
    delete cksResponse;

    if( elems.size() != 2 )
      return XRootDStatus( stError, errInvalidResponse );

    if( elems[0] != checkSumType )
      return XRootDStatus( stError, errCheckSumError );

    checkSum = elems[0] + ":";
    checkSum += elems[1];

    log->Dump( UtilityMsg, "Checksum for %s checksum: %s",
               path.c_str(), checkSum.c_str() );

    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Get a checksum from local file
  //------------------------------------------------------------------------
  XRootDStatus Utils::GetLocalCheckSum( std::string       &checkSum,
                                        const std::string &checkSumType,
                                        const std::string &path )
  {
    Log    *log    = DefaultEnv::GetLog();
    XrdCks *cksMan = DefaultEnv::GetCheckSumManager();

    if( !cksMan )
    {
      log->Error( UtilityMsg, "Unable to get the checksum manager" );
      return XRootDStatus( stError, errInternal );
    }

    XrdCksData ckSum; ckSum.Set( checkSumType.c_str() );
    int status = cksMan->Calc( path.c_str(), ckSum, 0 );
    if( status != 0 )
    {
      log->Error( UtilityMsg, "Error while calculating checksum for %s: %s",
                  path.c_str(), strerror( -status ) );
      return XRootDStatus( stError, errCheckSumError, -status );
    }

    char *cksBuffer = new char[265];
    ckSum.Get( cksBuffer, 256 );
    checkSum  = checkSumType + ":";
    checkSum += cksBuffer;
    delete [] cksBuffer;

    log->Dump( UtilityMsg, "Checksum for %s is: %s", path.c_str(),
               checkSum.c_str() );

    return XRootDStatus();
  }
}

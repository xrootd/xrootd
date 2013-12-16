//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUglyHacks.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksLoader.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksCalcmd5.hh"
#include "XrdCks/XrdCksCalccrc32.hh"
#include "XrdCks/XrdCksCalcadler32.hh"
#include "XrdVersion.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>

XrdVERSIONINFOREF( XrdCl );

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  CheckSumManager::CheckSumManager()
  {
    pLoader = new XrdCksLoader( XrdVERSIONINFOVAR( XrdCl ) );
    pCalculators["md5"]     = new XrdCksCalcmd5();
    pCalculators["crc32"]   = new XrdCksCalccrc32;
    pCalculators["adler32"] = new XrdCksCalcadler32;
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  CheckSumManager::~CheckSumManager()
  {
    CalcMap::iterator it;
    for( it = pCalculators.begin(); it != pCalculators.end(); ++it )
      delete it->second;
    delete pLoader;
  }

  //----------------------------------------------------------------------------
  // Get a calculator
  //----------------------------------------------------------------------------
  XrdCksCalc *CheckSumManager::GetCalculator( const std::string &algName )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );
    CalcMap::iterator it = pCalculators.find( algName );
    if( it == pCalculators.end() )
    {
      char *errBuff = new char[1024];
      log->Dump( UtilityMsg, "Attempting to load a calculator for: %s",
                 algName.c_str() );
      XrdCksCalc *c = pLoader->Load( algName.c_str(), "", errBuff, 1024 );
      if( !c )
      {
        log->Error( UtilityMsg, "Unable to load %s calculator: %s",
                    algName.c_str(), errBuff );
        delete [] errBuff;
        return 0;

      }
      delete [] errBuff;

      pCalculators[algName] = c;
      return c->New();
    }
    return it->second->New();;
  }

  //----------------------------------------------------------------------------
  // Stop the manager
  //----------------------------------------------------------------------------
  bool CheckSumManager::Calculate( XrdCksData        &result,
                                   const std::string &algName,
                                   const std::string &filePath )
  {
    //--------------------------------------------------------------------------
    // Get a calculator
    //--------------------------------------------------------------------------
    Log        *log  = DefaultEnv::GetLog();
    XrdCksCalc *calc = GetCalculator( algName );

    if( !calc )
    {
      log->Error( UtilityMsg, "Unable to get a calculator for %s",
                  algName.c_str() );
      return false;
    }
    XRDCL_SMART_PTR_T<XrdCksCalc> calcPtr( calc );

    //--------------------------------------------------------------------------
    // Open the file
    //--------------------------------------------------------------------------
    log->Debug( UtilityMsg, "Opening %s for reading (checksum calc)",
               filePath.c_str() );

    int fd = open( filePath.c_str(), O_RDONLY );
    if( fd == -1 )
    {
      log->Error( UtilityMsg, "Unable to open %s: %s", filePath.c_str(),
                  strerror( errno ) );
      return false;
    }

    //--------------------------------------------------------------------------
    // Calculate the checksum
    //--------------------------------------------------------------------------
    const uint32_t  buffSize   = 2*1024*1024;
    char           *buffer     = new char[buffSize];
    int64_t         bytesRead  = 0;

    while( (bytesRead = read( fd, buffer, buffSize )) )
    {
      if( bytesRead == -1 )
      {
        log->Error( UtilityMsg, "Unable read from %s: %s", filePath.c_str(),
                    strerror( errno ) );
        close( fd );
        delete [] buffer;
        return false;
      }
      calc->Update( buffer, bytesRead );
    }

    int size;
    calc->Type( size );
    result.Set( (void*)calc->Final(), size );

    //--------------------------------------------------------------------------
    // Clean up
    //--------------------------------------------------------------------------
    delete [] buffer;
    close( fd );
    return true;
  }
}

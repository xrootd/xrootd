/*
 * CheckSumHelper.hh
 *
 *  Created on: Sep 20, 2021
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLCHECKSUMHELPER_HH_
#define SRC_XRDCL_XRDCLCHECKSUMHELPER_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"

#include <string>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Check sum helper for stdio
  //----------------------------------------------------------------------------
  class CheckSumHelper
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CheckSumHelper( const std::string &name,
                      const std::string &ckSumType ):
        pName( name ),
        pCkSumType( ckSumType ),
        pCksCalcObj( 0 )
      {};

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CheckSumHelper()
      {
        delete pCksCalcObj;
      }

      //------------------------------------------------------------------------
      //! Initialize
      //------------------------------------------------------------------------
      XRootDStatus Initialize()
      {
        if( pCkSumType.empty() )
          return XRootDStatus();

        Log             *log    = DefaultEnv::GetLog();
        CheckSumManager *cksMan = DefaultEnv::GetCheckSumManager();

        if( !cksMan )
        {
          log->Error( UtilityMsg, "Unable to get the checksum manager" );
          return XRootDStatus( stError, errInternal );
        }

        pCksCalcObj = cksMan->GetCalculator( pCkSumType );
        if( !pCksCalcObj )
        {
          log->Error( UtilityMsg, "Unable to get a calculator for %s",
                      pCkSumType.c_str() );
          return XRootDStatus( stError, errCheckSumError );
        }

        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      // Update the checksum
      //------------------------------------------------------------------------
      void Update( const void *buffer, uint32_t size )
      {
        if( pCksCalcObj )
          pCksCalcObj->Update( (const char *)buffer, size );
      }

      //------------------------------------------------------------------------
      // Get checksum
      //------------------------------------------------------------------------
      XRootDStatus GetCheckSum( std::string &checkSum,
                                std::string &checkSumType )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        int calcSize = 0;
        auto st = GetCheckSumImpl( checkSumType, calcSize );
        if( !st.IsOK() ) return st;

        //----------------------------------------------------------------------
        // Response
        //----------------------------------------------------------------------
        XrdCksData ckSum;
        ckSum.Set( checkSumType.c_str() );
        ckSum.Set( (void*)pCksCalcObj->Final(), calcSize );
        char *cksBuffer = new char[265];
        ckSum.Get( cksBuffer, 256 );
        checkSum  = checkSumType + ":";
        checkSum += Utils::NormalizeChecksum( checkSumType, cksBuffer );
        delete [] cksBuffer;

        log->Dump( UtilityMsg, "Checksum for %s is: %s", pName.c_str(),
                   checkSum.c_str() );
        return XRootDStatus();
      }

      template<typename T>
      XRootDStatus GetRawCheckSum( const std::string &checkSumType, T &value )
      {
        int calcSize = 0;
        auto st = GetCheckSumImpl( checkSumType, calcSize );
        if( !st.IsOK() ) return st;
        if( sizeof( T ) != calcSize )
          return XRootDStatus( stError, errInvalidArgs, 0,
                                      "checksum size mismatch" );
        value = *reinterpret_cast<T*>( pCksCalcObj->Final() );
        return XRootDStatus();
      }

      const std::string& GetType()
      {
        return pCkSumType;
      }

    private:

      //------------------------------------------------------------------------
      // Get checksum
      //------------------------------------------------------------------------
      inline
      XRootDStatus GetCheckSumImpl( const std::string  &checkSumType,
                                    int                &calcSize )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Sanity check
        //----------------------------------------------------------------------
        if( !pCksCalcObj )
        {
          log->Error( UtilityMsg, "Calculator for %s was not initialized",
                      pCkSumType.c_str() );
          return XRootDStatus( stError, errCheckSumError );
        }

        std::string  calcType = pCksCalcObj->Type( calcSize );

        if( calcType != checkSumType )
        {
          log->Error( UtilityMsg, "Calculated checksum: %s, requested "
                      "checksum: %s", pCkSumType.c_str(),
                      checkSumType.c_str() );
          return XRootDStatus( stError, errCheckSumError );
        }

        return XRootDStatus();
      }

      std::string  pName;
      std::string  pCkSumType;
      XrdCksCalc  *pCksCalcObj;
  };
}


#endif /* SRC_XRDCL_XRDCLCHECKSUMHELPER_HH_ */

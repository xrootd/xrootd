/*
 * XrdZipError.hh
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPERROR_HH_
#define SRC_XRDZIP_XRDZIPERROR_HH_

namespace XrdZip
{
  //---------------------------------------------------------------------------
  //! An exception for carrying the XRootDStatus of InflCache
  //---------------------------------------------------------------------------
  struct Error : public std::exception
  {
      Error( const XrdCl::XRootDStatus &status ) : status( status )
      {
      }

      XrdCl::XRootDStatus status;
  };
}

#endif /* SRC_XRDZIP_XRDZIPERROR_HH_ */

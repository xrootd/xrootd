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

#ifndef __XRD_CL_STATUS_HH__
#define __XRD_CL_STATUS_HH__

#include <stdint.h>
#include <errno.h>
#include <sstream>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constants
  //----------------------------------------------------------------------------
  const uint16_t stOK    = 0x0000;  //!< Everything went OK
  const uint16_t stError = 0x0001;  //!< An error occurred that could potentially be retried
  const uint16_t stFatal = 0x0003;  //!< Fatal error, it's still an error

  //----------------------------------------------------------------------------
  // Additional info for the stOK status
  //----------------------------------------------------------------------------
  const uint16_t suDone            = 0;
  const uint16_t suContinue        = 1;
  const uint16_t suRetry           = 2;
  const uint16_t suPartial         = 3;
  const uint16_t suAlreadyDone     = 4;

  //----------------------------------------------------------------------------
  // Generic errors
  //----------------------------------------------------------------------------
  const uint16_t errNone           = 0; //!< No error
  const uint16_t errRetry          = 1; //!< Try again for whatever reason
  const uint16_t errUnknown        = 2; //!< Unknown error
  const uint16_t errInvalidOp      = 3; //!< The operation cannot be performed in the
                                        //!< given circumstances
  const uint16_t errFcntl          = 4; //!< failed manipulate file descriptor
  const uint16_t errPoll           = 5; //!< error while polling descriptors
  const uint16_t errConfig         = 6; //!< System misconfigured
  const uint16_t errInternal       = 7; //!< Internal error
  const uint16_t errUnknownCommand = 8;
  const uint16_t errInvalidArgs    = 9;
  const uint16_t errInProgress     = 10;
  const uint16_t errUninitialized  = 11;
  const uint16_t errOSError        = 12;
  const uint16_t errNotSupported   = 13;
  const uint16_t errDataError      = 14; //!< data is corrupted
  const uint16_t errNotImplemented = 15; //!< Operation is not implemented

  //----------------------------------------------------------------------------
  // Socket related errors
  //----------------------------------------------------------------------------
  const uint16_t errInvalidAddr        = 101;
  const uint16_t errSocketError        = 102;
  const uint16_t errSocketTimeout      = 103;
  const uint16_t errSocketDisconnected = 104;
  const uint16_t errPollerError        = 105;
  const uint16_t errSocketOptError     = 106;
  const uint16_t errStreamDisconnect   = 107;
  const uint16_t errConnectionError    = 108;
  const uint16_t errInvalidSession     = 109;

  //----------------------------------------------------------------------------
  // Post Master related errors
  //----------------------------------------------------------------------------
  const uint16_t errInvalidMessage     = 201;
  const uint16_t errHandShakeFailed    = 202;
  const uint16_t errLoginFailed        = 203;
  const uint16_t errAuthFailed         = 204;
  const uint16_t errQueryNotSupported  = 205;
  const uint16_t errOperationExpired   = 206;

  //----------------------------------------------------------------------------
  // XRootD related errors
  //----------------------------------------------------------------------------
  const uint16_t errNoMoreFreeSIDs     = 301;
  const uint16_t errInvalidRedirectURL = 302;
  const uint16_t errInvalidResponse    = 303;
  const uint16_t errNotFound           = 304;
  const uint16_t errCheckSumError      = 305;
  const uint16_t errRedirectLimit      = 306;

  const uint16_t errErrorResponse      = 400;
  const uint16_t errRedirect           = 401;

  const uint16_t errResponseNegative   = 500; //!< Query response was negative

  //----------------------------------------------------------------------------
  //! Procedure execution status
  //----------------------------------------------------------------------------
  struct Status
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    Status( uint16_t st = stOK, uint16_t cod = errNone, uint32_t errN = 0 ):
      status(st), code(cod), errNo( errN ) {}

    bool IsError() const { return status & stError; }           //!< Error
    bool IsFatal() const { return (status&0x0002) & stFatal; }  //!< Fatal error
    bool IsOK()    const { return status == stOK; }             //!< We're fine

    //--------------------------------------------------------------------------
    //! Get the status code that may be returned to the shell
    //--------------------------------------------------------------------------
    int GetShellCode() const
    {
      if( IsOK() )
        return 0;
      return (code/100)+50;
    }

    //--------------------------------------------------------------------------
    //! Create a string representation
    //--------------------------------------------------------------------------
    std::string ToString() const;

    uint16_t status;     //!< Status of the execution
    uint16_t code;       //!< Error type, or additional hints on what to do
    uint32_t errNo;      //!< Errno, if any
  };
}

#endif // __XRD_CL_STATUS_HH__

//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_STATUS_HH__
#define __XRD_CL_STATUS_HH__

#include <stdint.h>
#include <errno.h>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Constants
  //----------------------------------------------------------------------------
  const uint16_t stOK    = 0x0000;  //!< Everything went OK
  const uint16_t stError = 0x0001;  //!< An error occured that could potentially be retried
  const uint16_t stFatal = 0x0003;  //!< Fatal error, it's still an error

  //----------------------------------------------------------------------------
  // Additional info for the stOK status
  //----------------------------------------------------------------------------
  const uint16_t suDone            = 0;
  const uint16_t suContinue        = 1;
  const uint16_t suRetry           = 2;

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

  //----------------------------------------------------------------------------
  // Post Master related errors
  //----------------------------------------------------------------------------
  const uint16_t errInvalidMessage     = 201;
  const uint16_t errHandShakeFailed    = 202;
  const uint16_t errLoginFailed        = 203;
  const uint16_t errAuthFailed         = 203;

  //----------------------------------------------------------------------------
  // XRootD related errors
  //----------------------------------------------------------------------------
  const uint16_t errNoMoreFreeSIDs     = 301;

  //----------------------------------------------------------------------------
  //! Proceure execution status
  //----------------------------------------------------------------------------
  struct Status
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    Status( uint16_t st = stOK, uint16_t cod = errNone, int errN = 0 ):
      status(st), code(cod), errNo( errN ) {}

    bool IsError() { return status & stError; }
    bool IsFatal() { return (status&0x0002) & stFatal; }
    bool IsOK()    { return status == stOK; }

    uint16_t status;     //!< Status of the execution
    uint16_t code;       //!< Error type, or additional hints on what to do
    int      errNo;      //!< Errno, if any
  };
}

#endif // __XRD_CL_STATUS_HH__

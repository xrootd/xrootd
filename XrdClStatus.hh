//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
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
  uint16_t stOK    = 0x0000;  //!< Everything went OK
  uint16_t stError = 0x0001;  //!< An error occured that could potentially be retried
  uint16_t stFatal = 0x0003;  //!< Fatal error, it's still an error

  //----------------------------------------------------------------------------
  // Generic errors
  //----------------------------------------------------------------------------
  uint16_t errNone           = 0; //!< No error
  uint16_t errInvalidOp      = 1; //!< The operation cannot be performed in the
                                  //!< given circumstances
  uint16_t errFcntl          = 2; //!< failed manipulate file descriptor
  uint16_t errPoll           = 3; //!< error while polling descriptors

  //----------------------------------------------------------------------------
  // Socket related errors
  //----------------------------------------------------------------------------
  uint16_t errInvalidAddr        = 101;
  uint16_t errSocketError        = 102;
  uint16_t errSocketTimeout      = 103;
  uint16_t errSocketDisconnected = 104;

  //----------------------------------------------------------------------------
  //! Proceure execution status
  //----------------------------------------------------------------------------
  struct Status
  {
    Status( uint16_t st = stOK, uint16_t err = errNone, int errN = 0 ):
      status(st), errorType(err), errNo( errN ) {}
    uint16_t status;
    uint16_t errorType;
    int      errNo;
  };
}

#endif // __XRD_CL_STATUS_HH__

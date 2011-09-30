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
  const uint16_t stOK    = 0x0000;  //!< Everything went OK
  const uint16_t stError = 0x0001;  //!< An error occured that could potentially be retried
  const uint16_t stFatal = 0x0003;  //!< Fatal error, it's still an error

  //----------------------------------------------------------------------------
  // Generic errors
  //----------------------------------------------------------------------------
  const uint16_t errNone           = 0; //!< No error
  const uint16_t errUnknown        = 1;
  const uint16_t errInvalidOp      = 2; //!< The operation cannot be performed in the
                                        //!< given circumstances
  const uint16_t errFcntl          = 3; //!< failed manipulate file descriptor
  const uint16_t errPoll           = 4; //!< error while polling descriptors

  //----------------------------------------------------------------------------
  // Socket related errors
  //----------------------------------------------------------------------------
  const uint16_t errInvalidAddr        = 101;
  const uint16_t errSocketError        = 102;
  const uint16_t errSocketTimeout      = 103;
  const uint16_t errSocketDisconnected = 104;

  //----------------------------------------------------------------------------
  //! Proceure execution status
  //----------------------------------------------------------------------------
  struct Status
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    Status( uint16_t st = stOK, uint16_t err = errNone, int errN = 0 ):
      status(st), errorType(err), errNo( errN ) {}

    uint16_t status;     //!< Status of the execution
    uint16_t errorType;  //!< Error type, if any
    int      errNo;      //!< Errno, if any
  };
}

#endif // __XRD_CL_STATUS_HH__

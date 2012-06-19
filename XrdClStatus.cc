//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClStatus.hh"
#include <cstring>

namespace
{
  using namespace XrdCl;
  struct ErrorMap
  {
    uint16_t    code;
    const char *msg;
  };

  ErrorMap errors[] = {
    { errUnknown,            "Unknown error"        },
    { errInvalidOp,          "Invalid operaion"     },
    { errFcntl,              "Fcntl error"          },
    { errPoll,               "Poll error"           },
    { errConfig,             "Configuration error"  },
    { errInternal,           "Internal error"       },
    { errUnknownCommand,     "Command not found"    },
    { errInvalidArgs,        "Invalid arguments"    },
    { errInProgress,         "Operation in progress" },
    { errUninitialized,      "Initialization error" },
    { errOSError,            "OS Error"             },
    { errInvalidAddr,        "Invalid address"      },
    { errSocketError,        "Socket error"         },
    { errSocketTimeout,      "Socket timeout"       },
    { errSocketDisconnected, "Socket disconnected"  },
    { errPollerError,        "Poller error"         },
    { errSocketOptError,     "Socket opt error"     },
    { errStreamDisconnect,   "Stream disconnect"    },
    { errConnectionError,    "Conection error"      },
    { errInvalidMessage,     "Invalid message"      },
    { errNotFound,           "Resource not found"   },
    { errHandShakeFailed,    "Hand shake failed"    },
    { errLoginFailed,        "Login failed"         },
    { errAuthFailed,         "Auth failed"          },
    { errQueryNotSupported,  "Query not supported"  },
    { errNoMoreFreeSIDs,     "No more free SIDs"    },
    { errInvalidRedirectURL, "Invalid redirect URL" },
    { errInvalidResponse,    "Invalid response"     },
    { errErrorResponse,      "Error response"       },
    { 0, 0 } };

  //----------------------------------------------------------------------------
  // Get error code
  //----------------------------------------------------------------------------
  std::string GetErrorMessage( uint16_t code )
  {
    for( int i = 0; errors[i].msg != 0; ++i )
      if( errors[i].code == code )
        return errors[i].msg;
    return "Unknown error code";
  }
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Create a string representation
  //----------------------------------------------------------------------------
  std::string Status::ToString() const
  {
    std::ostringstream o;

    //--------------------------------------------------------------------------
    // The status is OK
    //--------------------------------------------------------------------------
    if( IsOK() )
    {
      o << "[SUCCESS] ";

      if( code == suContinue )
        o << "Continue";
      else if( code == suRetry )
        o << "Retry";

      return o.str();
    }

    //--------------------------------------------------------------------------
    // We have an error
    //--------------------------------------------------------------------------
    if( IsFatal() )
      o << "[FATAL] ";
    else
      o << "[ERROR] ";

    o << GetErrorMessage( code );

    //--------------------------------------------------------------------------
    // Add errno
    //--------------------------------------------------------------------------
    if( errNo )
      o << ": " << strerror( errNo );
    return o.str();
  }
}

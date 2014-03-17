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
    { errInvalidOp,          "Invalid operation"    },
    { errFcntl,              "Fcntl error"          },
    { errPoll,               "Poll error"           },
    { errConfig,             "Configuration error"  },
    { errInternal,           "Internal error"       },
    { errUnknownCommand,     "Command not found"    },
    { errInvalidArgs,        "Invalid arguments"    },
    { errInProgress,         "Operation in progress" },
    { errUninitialized,      "Initialization error" },
    { errOSError,            "OS Error"             },
    { errNotSupported,       "Operation not supported" },
    { errDataError,          "Received corrupted data" },
    { errNotImplemented,     "Operation is not implemented" },
    { errInvalidAddr,        "Invalid address"      },
    { errSocketError,        "Socket error"         },
    { errSocketTimeout,      "Socket timeout"       },
    { errSocketDisconnected, "Socket disconnected"  },
    { errPollerError,        "Poller error"         },
    { errSocketOptError,     "Socket opt error"     },
    { errStreamDisconnect,   "Stream disconnect"    },
    { errConnectionError,    "Connection error"     },
    { errInvalidSession,     "Invalid session"      },
    { errInvalidMessage,     "Invalid message"      },
    { errNotFound,           "Resource not found"   },
    { errCheckSumError,      "CheckSum error"       },
    { errRedirectLimit,      "Redirect limit has been reached" },
    { errHandShakeFailed,    "Hand shake failed"    },
    { errLoginFailed,        "Login failed"         },
    { errAuthFailed,         "Auth failed"          },
    { errQueryNotSupported,  "Query not supported"  },
    { errOperationExpired,   "Operation expired"    },
    { errNoMoreFreeSIDs,     "No more free SIDs"    },
    { errInvalidRedirectURL, "Invalid redirect URL" },
    { errInvalidResponse,    "Invalid response"     },
    { errRedirect,           "Unhandled redirect"   },
    { errErrorResponse,      "Error response"       },
    { errResponseNegative,   "Query response negative" },
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

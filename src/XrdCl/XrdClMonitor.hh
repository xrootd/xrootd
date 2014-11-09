//------------------------------------------------------------------------------
// Copyright (c) 2012 by the Board of Trustees of the Leland Stanford, Jr.,
// University
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Andrew Hanushevsky <abh@stanford.edu>
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

//------------------------------------------------------------------------------
//! When the envar XRD_CLIENTMONITOR is set to the libpath/libname.so that
//! holds the monitoring object, it is automatically loaded. The following
//! "C" external symbols must exist in the monitor plug-in shared library.
//! It is called to obtain an instance of the XrdCl::Monitor object.
//!
//! @param  exec  full path name to executable provided by XrdSysUtils::ExecName
//!
//! @param  parms Value of XRD_CLIENTMONITOPARAM envar or null if it is not set.
//!
//! @return Pointer to an instance of XrdCl::Monitor or null which causes
//!         monitoring to be disabled.
//!
//! extern "C"
//! {
//!   XrdCl::Monitor *XrdClGetMonitor(const char *exec, const char *parms);
//! }
//------------------------------------------------------------------------------

#ifndef __XRD_CL_MONITOR_HH__
#define __XRD_CL_MONITOR_HH__

#include "XrdCl/XrdClFileSystem.hh"

namespace XrdCl
{
  class URL;

  //----------------------------------------------------------------------------
  //! An abstract class to describe the client-side monitoring plugin interface.
  //----------------------------------------------------------------------------
  class Monitor
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Monitor() {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Monitor() {}

      //------------------------------------------------------------------------
      //! Describe a server login event
      //------------------------------------------------------------------------
      struct ConnectInfo
      {
        ConnectInfo(): streams( 0 )
        {
          sTOD.tv_sec = 0; sTOD.tv_usec = 0;
          eTOD.tv_sec = 0; eTOD.tv_usec = 0;
        }
        std::string server;  //!< user@host:port
        std::string auth;    //!< authentication protocol used or empty if none
        timeval     sTOD;    //!< gettimeofday() when login started
        timeval     eTOD;    //!< gettimeofday() when login ended
        uint16_t    streams; //!< Number of streams
      };

      //------------------------------------------------------------------------
      //! Describe a server logout event
      //------------------------------------------------------------------------
      struct DisconnectInfo
      {
        DisconnectInfo(): rBytes(0), sBytes(0), cTime(0)
        {}
        std::string server;  //!< user@host:port
        uint64_t    rBytes;  //!< Number of bytes received
        uint64_t    sBytes;  //!< Number of bytes sent
        time_t      cTime;   //!< Seconds connected to the server
        Status      status;  //!< Disconnection status
      };

      //------------------------------------------------------------------------
      //! Describe a file open event to the monitor
      //------------------------------------------------------------------------
      struct OpenInfo
      {
        OpenInfo(): file(0), fSize(0), oFlags(0) {}
        const URL   *file;        //!< File in question
        std::string  dataServer;  //!< Actual fata server
        uint64_t     fSize;       //!< File size in bytes
        uint16_t     oFlags;      //!< OpenFlags
      };

      //------------------------------------------------------------------------
      //! Describe a file close event
      //------------------------------------------------------------------------
      struct CloseInfo
      {
        CloseInfo():
          file(0), rBytes(0), vBytes(0), wBytes(0), vSegs(0), rCount(0),
          vCount(0), wCount(0), status(0)
        {
          oTOD.tv_sec = 0; oTOD.tv_usec = 0;
          cTOD.tv_sec = 0; cTOD.tv_usec = 0;
        }
        const URL          *file;    //!< The file in question
        timeval             oTOD;    //!< gettimeofday() when file was opened
        timeval             cTOD;    //!< gettimeofday() when file was closed
        uint64_t            rBytes;  //!< Total number of bytes read via read
        uint64_t            vBytes;  //!< Total number of bytes read via readv
        uint64_t            wBytes;  //!< Total number of bytes written
        uint64_t            vSegs;   //!< Total count  of readv segments
        uint32_t            rCount;  //!< Total count  of reads
        uint32_t            vCount;  //!< Total count  of readv
        uint32_t            wCount;  //!< Total count  of writes
        const XRootDStatus *status;  //!< Close status
      };

      //------------------------------------------------------------------------
      //! Describe an encountered file-based error
      //------------------------------------------------------------------------
      struct ErrorInfo
      {
        enum Operation
        {
          ErrOpen = 0,  //!< Open (ditto)
          ErrRead,      //!< Read
          ErrReadV,     //!< Readv
          ErrWrite,     //!< Write
          ErrUnc        //!< Unclassified operation
        };

        ErrorInfo(): file(0), status(0), opCode( ErrUnc ) {}
        const URL          *file;    //!< The file in question
        const XRootDStatus *status;  //!< Status code
        Operation           opCode;  //!< The associated operation
      };

      //------------------------------------------------------------------------
      //! Describe the transfer
      //------------------------------------------------------------------------
      struct TransferInfo
      {
        TransferInfo(): origin(0), target(0) {}
        const URL *origin;  //!< URL of the origin
        const URL *target;  //!< URL of the target
      };

      //------------------------------------------------------------------------
      //! Describe a start of copy event. Copy events are sequential by nature.
      //! a copybeg event is followed by a number of open and close events. When
      //! the copy finishes, all files are closed and a copyend event occurs.
      //------------------------------------------------------------------------
      struct CopyBInfo
      {
        TransferInfo transfer; //!< The transfer in question
      };

      //------------------------------------------------------------------------
      //! Describe an end  of copy event
      //------------------------------------------------------------------------
      struct CopyEInfo
      {
        CopyEInfo(): sources(0), status(0)
        {
          bTOD.tv_sec = 0; bTOD.tv_usec = 0;
          eTOD.tv_sec = 0; eTOD.tv_usec = 0;
        }
        TransferInfo        transfer; //!< The transfer in question
        int                 sources;  //!< Number of sources used for the copy
        timeval             bTOD;     //!< Copy start time
        timeval             eTOD;     //!< Copy end time
        const XRootDStatus *status;   //!< Status of the copy
      };

      //------------------------------------------------------------------------
      //! Describe a checksum event
      //------------------------------------------------------------------------
      struct CheckSumInfo
      {
        CheckSumInfo(): oTime(0), tTime(0), isOK(false) {}
        TransferInfo transfer;  //!< The transfer in question
        std::string  cksum;     //!< Checksum as <type>:<value>
        uint64_t     oTime;     //!< Microseconds to obtain cksum from origin
        uint64_t     tTime;     //!< Microseconds to obtain cksum from target
        bool         isOK;      //!< True if checksum matched, false otherwise
      };

      //------------------------------------------------------------------------
      //! Event codes passed to the Event() method. Event code values not
      //! listed here, if encountered, should be ignored.
      //------------------------------------------------------------------------
      enum EventCode
      {
        EvCopyBeg,        //!< CopyBInfo: Copy operation started
        EvCopyEnd,        //!< CopyEInfo: Copy operation ended
        EvCheckSum,       //!< CheckSumInfo: File checksummed
        EvOpen,           //!< OpenInfo: File opened
        EvClose,          //!< CloseInfo: File closed
        EvErrIO,          //!< ErrorInfo: An I/O error occurred
        EvConnect,        //!< ConnectInfo: Login  into a server
        EvDisconnect      //!< DisconnectInfo: Logout from a server

      };

      //------------------------------------------------------------------------
      //! Inform the monitor of an event.
      //!
      //! @parm evCode  is the event that occurred (see enum evNum)
      //! @parm evInfo  is the event information structure describing the event
      //!               it is cast to (void *) so that one method can be used
      //!               and should be recast to the correct corresponding struct
      //------------------------------------------------------------------------
      virtual void Event( EventCode evCode, void *evData ) = 0;
  };
}

#endif // __XRD_CL_MONITOR_HH

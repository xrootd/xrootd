/*
 * XrdClFifo.hh
 *
 *  Created on: Jun 4, 2021
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLFIFO_HH_
#define SRC_XRDCL_XRDCLFIFO_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysE2T.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace XrdCl
{
  class Fifo
  {
    public:

      static inline XRootDStatus Create( const std::string &path )
      {
        int rc = mkfifo( path.c_str(), 0600 );
        if( rc == -1 )
          return XRootDStatus( stError, errOSError, errno, XrdSysE2T( errno ) );
        return XRootDStatus();
      }

      inline Fifo() : fifo( -1 )
      {
      }

      inline XRootDStatus Open( const std::string &path, OpenFlags::Flags flags )
      {
        int flg = O_NONBLOCK;
        if( flags & OpenFlags::Read ) flg |= O_RDONLY;
        else if( flags & OpenFlags::Write ) flg |= O_WRONLY;
        fifo = XrdSysFD_Open( path.c_str(), flg );
        if( fifo == -1 )
          return XRootDStatus( stError, errOSError, errno, XrdSysE2T( errno ) );
        return XRootDStatus();
      }

      inline XRootDStatus Close()
      {
        int rc = close( fifo );
        if( rc == -1 )
          return XRootDStatus( stError, errOSError, errno, XrdSysE2T( errno ) );
        return XRootDStatus();
      }

      template<typename INT>
      inline XRootDStatus Put( INT &value )
      {
        ssize_t rc = write( fifo, &value, sizeof( INT ) );
        if( rc == sizeof( INT ) ) return XRootDStatus();
        if( rc == -1 )
          return XRootDStatus( stError, errOSError, errno, XrdSysE2T( errno ) );
        return XRootDStatus( stError, errOSError, 0, "Failed to write value to fifo." );
      }

      template<typename INT>
      inline XRootDStatus Get( INT &value )
      {
        ssize_t rc = read( fifo, &value, sizeof( INT ) );
        if( rc == sizeof( INT ) ) return XRootDStatus();
        if( rc == 0 ) return XRootDStatus( stOK, suRetry );
        if( rc == -1 && errno == EAGAIN ) return XRootDStatus( stOK, suRetry );
        if( rc == -1 )
          return XRootDStatus( stError, errOSError, errno, XrdSysE2T( errno ) );
        return XRootDStatus( stError, errOSError, 0, "Failed to read value from fifo." );
      }

    private:

      int fifo;
  };
}


#endif /* SRC_XRDCL_XRDCLFIFO_HH_ */

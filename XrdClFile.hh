//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_FILE_HH__
#define __XRD_CL_FILE_HH__

#include <stdint.h>
#include <string>

namespace XrdClient
{
  class ProtocolHandler;

  //----------------------------------------------------------------------------
  //! A file
  //----------------------------------------------------------------------------
  class File
  {
    public:
      //------------------------------------------------------------------------
      // Open modes
      //------------------------------------------------------------------------
      static const uint16_t  OpMkPath = 0x0001;
      static const uint16_t  OpNew    = 0x0002; 
      static const uint16_t  OpAppend = 0x0004;
      static const uint16_t  OpRead   = 0x0008;
      static const uint16_t  OpUpdate = 0x0010;

      //------------------------------------------------------------------------
      //! Interface for a read callback
      //------------------------------------------------------------------------
      class ReadCallback
      {
        public:
          virtual void ReadStatus( bool success, int fileID, uint64_t offset,
                                   uint32_t size, char *data ) = 0;
      };

      //------------------------------------------------------------------------
      //! Interface for a write callback
      //------------------------------------------------------------------------
      class WriteCallback
      {
        public:
          virtual void WriteStatus( bool success, int fileID, uint64_t offset,
                                    uint32_t size ) = 0;
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      File(): pFileID(-1), pHandler(0) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~File() { Close(); }

      //------------------------------------------------------------------------
      //! Open the file pointed to by the given URL
      //------------------------------------------------------------------------
      bool Open( const std::string &url, uint16_t mode );

      //------------------------------------------------------------------------
      //! Close the file
      //------------------------------------------------------------------------
      bool Close();

      //------------------------------------------------------------------------
      //! Synchronously read data at given offset
      //------------------------------------------------------------------------
      bool Read( uint64_t offset, uint32_t size, void *buffer );

      //------------------------------------------------------------------------
      //! Send a data read request and call the given callback when the data
      //! is available
      //------------------------------------------------------------------------
      bool Read( uint64_t offset, uint32_t size, ReadCallback *callBack );

      //------------------------------------------------------------------------
      //! Synchronously write data at given offset
      //------------------------------------------------------------------------
      bool Write( uint64_t offset, uint32_t size, void *buffer );

      //------------------------------------------------------------------------
      //! Send a data write request and call the given callback when the
      //! confirmation has been received.
      //------------------------------------------------------------------------
      bool Write( uint64_t offset, uint32_t size, WriteCallback *callBack );

    private:
      int              pFileID;
      ProtocolHandler *pHandler;
  };
}

#endif // __XRD_CL_FILE_HH__

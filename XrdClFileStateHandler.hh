//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_FILE_STATE_HANDLER_HH__
#define __XRD_CL_FILE_STATE_HANDLER_HH__

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdSys/XrdSysPthread.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Handle the statefull operations
  //----------------------------------------------------------------------------
  class FileStateHandler
  {
    public:
      //------------------------------------------------------------------------
      //! State of the file
      //------------------------------------------------------------------------
      enum FileStatus
      {
        Closed,           //! The file is closed
        Opened,           //! Opening has succeeded
        Error,            //! Opening has failed
        OpenInProgress,   //! Opening is in progress
        CloseInProgress   //! Closing operation is in progress
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      FileStateHandler();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~FileStateHandler();

      //------------------------------------------------------------------------
      //! Open the file pointed to by the given URL
      //!
      //! @param url     url of the file to be opened
      //! @param flags   OpenFlags::Flags
      //! @param mode    Access::Mode for new files, 0 otherwise
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Open( const std::string &url,
                         uint16_t           flags,
                         uint16_t           mode,
                         ResponseHandler   *handler,
                         uint16_t           timeout  = 0 );

      //------------------------------------------------------------------------
      //! Close the file object
      //!
      //! @param handler handler to be notified about the status of the operation
      //! @param timeout timeout value, if 0 the environment default will be
      //!                used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Close( ResponseHandler *handler,
                          uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Process the results of the opening operation
      //------------------------------------------------------------------------
      void SetOpenStatus( const XRootDStatus             *status,
                          const OpenInfo                 *openInfo,
                          const ResponseHandler::URLList *hostList );

      //------------------------------------------------------------------------
      //! Process the results of the closing operation
      //------------------------------------------------------------------------
      void SetCloseStatus( const XRootDStatus *status );

    private:
      XrdSysMutex   pMutex;
      FileStatus    pFileState;
      XRootDStatus  pStatus;
      StatInfo     *pStatInfo;
      URL          *pFileUrl;
      URL          *pDataServer;
      URL          *pLoadBalancer;
      uint8_t      *pFileHandle;
  };
}

#endif // __XRD_CL_FILE_STATE_HANDLER_HH__

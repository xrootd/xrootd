/*
 * XrdClMetalinkCopy.h
 *
 *  Created on: Sep 2, 2015
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLMETALINKCOPYJOB_HH_
#define SRC_XRDCL_XRDCLMETALINKCOPYJOB_HH_

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClCopyJob.hh"
#include "XrdCl/XrdClLog.hh"

#include "XrdOuc/XrdOucFileInfo.hh"

namespace XrdCl
{

  class MetalinkCopyJob : public CopyJob
  {
  public:
    //------------------------------------------------------------------------
    // Constructor
    //------------------------------------------------------------------------
    MetalinkCopyJob( uint16_t      jobId,
                    PropertyList *jobProperties,
                    PropertyList *jobResults );

    //------------------------------------------------------------------------
    //! Run the copy job
    //!
    //! @param progress the handler to be notified about the copy progress
    //! @return         status of the copy operation
    //------------------------------------------------------------------------
    virtual XRootDStatus Run( CopyProgressHandler *progress = 0 );

    //------------------------------------------------------------------------
    // Destructor
    //------------------------------------------------------------------------
    virtual ~MetalinkCopyJob();

  private:

    XRootDStatus CopyFiles( CopyProgressHandler *progress );
    XRootDStatus DownloadMetalink( CopyProgressHandler *progress );
    XRootDStatus ParseMetalink();
    XRootDStatus RemoveFile(const URL & url);

    uint16_t pJobId;
    XrdOucFileInfo ** pFileInfos;
    int size;
    std::string pMetalinkFile;
    bool pLocalFile;
  };

}

#endif /* SRC_XRDCL_XRDCLMETALINKCOPYJOB_HH_ */

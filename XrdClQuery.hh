//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_QUERY_HH__
#define __XRD_CL_QUERY_HH__

#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XProtocol/XProtocol.hh"
#include <string>
#include <vector>

namespace XrdClient
{
  class PostMaster;
  class Message;


  //----------------------------------------------------------------------------
  //! XRootD query request codes
  //----------------------------------------------------------------------------
  struct QueryCode
  {
    //--------------------------------------------------------------------------
    //! XRootD query request codes
    //--------------------------------------------------------------------------
    enum Code
    {
      Config         = kXR_Qconfig,    //!< Query server configuration
      ChecksumCancel = kXR_Qckscan,    //!< Query file checksum cancelation
      Checksum       = kXR_Qcksum,     //!< Query file checksum
      Opaque         = kXR_Qopaque,    //!< Implementation dependent
      OpaqueFile     = kXR_Qopaquf,    //!< Implementation dependent
      Prepare        = kXR_QPrep,      //!< Query prepare status
      Space          = kXR_Qspace,     //!< Query logical space stats
      Stats          = kXR_QStats,     //!< Query server stats
      Visa           = kXR_Qvisa,      //!< Query file visa attributes
      XAttr          = kXR_Qxattr      //!< Query file extended attributes
    };
  };

  //----------------------------------------------------------------------------
  //! Open flags, may be or'd when appropriate
  //----------------------------------------------------------------------------
  struct OpenFlags
  {
    //--------------------------------------------------------------------------
    //! Open flags, may be or'd when appropriate
    //--------------------------------------------------------------------------
    enum Flags
    {
      NoWait  = kXR_nowait,      //!< Provide a location as soon as one becomes
                                 //!< known. This means that not all locations
                                 //!< are necessarily returned. If the file
                                 //!< does not exist a wait is still imposed.
      Refresh = kXR_refresh,     //!< Refresh the cached information on file's
                                 //!< location. Voids NoWait.
    };
  };

  //----------------------------------------------------------------------------
  //! Send file/filesystem queries to an XRootD cluster
  //----------------------------------------------------------------------------
  class Query
  {
    public:
      typedef std::vector<LocationInfo> LocationList; //!< Location list

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url URL    of the entry-point server to be contacted
      //! @param postMaster post master to be used, if unspecified the default
      //!                   is used
      //------------------------------------------------------------------------
      Query( const URL &url, PostMaster *postMaster = 0 );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Query();

      //------------------------------------------------------------------------
      //! Locate a file - async
      //!
      //! @param path    path to the file to be located
      //! @param flags   some of the OpenFlags::Flags
      //! @param handler a handler to be notified when the response arrives,
      //!                the response parameter will hold a Buffer object
      //!                if the procedure is successfull
      //! @param timeout a timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Locate( const std::string &path,
                           uint16_t           flags,
                           ResponseHandler   *handler,
                           uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Locate a file - sync
      //!
      //! @param path     path to the file to be located
      //! @param flags    some of the OpenFlags::Flags
      //! @param response the response (to be deleted by the user
      //! @param timeout  a timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Locate( const std::string  &path,
                           uint16_t            flags,
                           LocationInfo      *&response,
                           uint16_t            timeout  = 0 );

      //------------------------------------------------------------------------
      //! Move a directory or a file - async
      //!
      //! @param source  the file or directory to be moved
      //! @param dest    the new name
      //! @param handler a handler to be notified when the response arrives,
      //! @param timeout a timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Mv( const std::string &source,
                       const std::string &dest,
                       ResponseHandler   *handler,
                       uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Move a directory or a file - sync
      //!
      //! @param source  the file or directory to be moved
      //! @param dest    the new name
      //! @param timeout a timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Mv( const std::string &source,
                       const std::string &dest,
                       uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain server information - async
      //!
      //! @param queryCode the query code as specified in the QueryCode struct
      //! @param arg       query argument
      //! @param handler   a handler to be notified when the response arrives,
      //!                  the response parameter will hold a Buffer object
      //!                  if the procedure is successfull
      //! @param timeout   a timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ServerQuery( QueryCode::Code  queryCode,
                                const Buffer    &arg,
                                ResponseHandler *handler,
                                uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain server information - sync
      //!
      //! @param queryCode the query code as specified in the QueryCode struct
      //! @param arg       query argument
      //! @param response  the response (to be deletedy by the user)
      //! @param timeout   a timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ServerQuery( QueryCode::Code   queryCode,
                                const Buffer     &arg,
                                Buffer          *&response,
                                uint16_t          timeout = 0 );

    private:

      //------------------------------------------------------------------------
      // Send a message and wait for a response
      //------------------------------------------------------------------------
      Status SendMessage( Message         *msg,
                          ResponseHandler *response,
                          uint16_t         timeout );

      PostMaster *pPostMaster;
      URL        *pUrl;
  };
}

#endif // __XRD_CL_QUERY_HH__

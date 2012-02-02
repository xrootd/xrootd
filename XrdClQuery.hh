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
      None    = 0,               //!< Nothing
      NoWait  = kXR_nowait,      //!< Provide a location as soon as one becomes
                                 //!< known. This means that not all locations
                                 //!< are necessarily returned. If the file
                                 //!< does not exist a wait is still imposed.
      Refresh = kXR_refresh,     //!< Refresh the cached information on file's
                                 //!< location. Voids NoWait.
    };
  };

  //----------------------------------------------------------------------------
  //! Access mode
  //----------------------------------------------------------------------------
  struct Access
  {
    enum Mode
    {
      UR = kXR_ur,         //!< owner readable
      UW = kXR_uw,         //!< owner writable
      UX = kXR_ux,         //!< owner executable/browsable
      GR = kXR_gr,         //!< group readable
      GW = kXR_gw,         //!< group writable
      GX = kXR_gx,         //!< group executable/browsable
      OR = kXR_or,         //!< world readable
      OW = kXR_ow,         //!< world writeable
      OX = kXR_ox,         //!< world executable/browsable
    };
  };

  //----------------------------------------------------------------------------
  //! MkDir flags
  //----------------------------------------------------------------------------
  struct MkDirFlags
  {
    static const uint8_t None     = 0;  //!< Nothing special
    static const uint8_t MakePath = 1;  //!< create the entire directory tree if
                                        //!< it doesn't exist
  };

  //----------------------------------------------------------------------------
  //! DirList flags
  //----------------------------------------------------------------------------
  struct DirListFlags
  {
    static const uint8_t None   = 0;  //!< Nothing special
    static const uint8_t Stat   = 1;  //!< Stat each entry
    static const uint8_t Locate = 2;  //!< Locate all servers hosting the
                                      //!< directory and send the dirlist
                                      //!< request to all of them
  };

  //----------------------------------------------------------------------------
  //! Stat flags
  //----------------------------------------------------------------------------
  struct StatFlags
  {
    static const uint8_t Object = 0;  //!< Do a file/directory stat
    static const uint8_t VFS    = 1;  //!< Stat virtual filesystem
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
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a Buffer object
      //!                if the procedure is successfull
      //! @param timeout timeout value, if 0 the environment default will
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
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Locate( const std::string  &path,
                           uint16_t            flags,
                           LocationInfo      *&response,
                           uint16_t            timeout  = 0 );

      //------------------------------------------------------------------------
      //! Locate a file, recursively locate all disk servers - async
      //!
      //! @param path    path to the file to be located
      //! @param flags   some of the OpenFlags::Flags
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a Buffer object
      //!                if the procedure is successfull
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus DeepLocate( const std::string &path,
                               uint16_t           flags,
                               ResponseHandler   *handler,
                               uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Locate a file, recursively locate all disk servers - sync
      //!
      //! @param path     path to the file to be located
      //! @param flags    some of the OpenFlags::Flags
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus DeepLocate( const std::string  &path,
                               uint16_t            flags,
                               LocationInfo      *&response,
                               uint16_t            timeout  = 0 );

      //------------------------------------------------------------------------
      //! Move a directory or a file - async
      //!
      //! @param source  the file or directory to be moved
      //! @param dest    the new name
      //! @param handler handler to be notified when the response arrives,
      //! @param timeout timeout value, if 0 the environment default will
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
      //! @param timeout timeout value, if 0 the environment default will
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
      //! @param handler   handler to be notified when the response arrives,
      //!                  the response parameter will hold a Buffer object
      //!                  if the procedure is successfull
      //! @param timeout   timeout value, if 0 the environment default will
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
      //! @param timeout   timeout value, if 0 the environment default will
      //!                  be used
      //! @return          status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ServerQuery( QueryCode::Code   queryCode,
                                const Buffer     &arg,
                                Buffer          *&response,
                                uint16_t          timeout = 0 );

      //------------------------------------------------------------------------
      //! Truncate a file - async
      //!
      //! @param path     path to the file to be truncated
      //! @param size     file size
      //! @param handler  handler to be notified when the response arrives
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Truncate( const std::string &path,
                             uint64_t           size,
                             ResponseHandler   *handler,
                             uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Truncate a file - sync
      //!
      //! @param path     path to the file to be truncated
      //! @param size     file size
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Truncate( const std::string &path,
                             uint64_t           size,
                             uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Remove a file - async
      //!
      //! @param path     path to the file to be removed
      //! @param handler  handler to be notified when the response arrives
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Rm( const std::string &path,
                       ResponseHandler   *handler,
                       uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Remove a file - sync
      //!
      //! @param path     path to the file to be removed
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Rm( const std::string &path,
                       uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Create a directory - async
      //!
      //! @param path     path to the directory
      //! @param flags    or'd MkDirFlags
      //! @param mode     access mode, or'd AccessMode::Mode
      //! @param handler  handler to be notified when the response arrives
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus MkDir( const std::string &path,
                          uint8_t            flags,
                          uint16_t           mode,
                          ResponseHandler   *handler,
                          uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Create a directory - sync
      //!
      //! @param path     path to the directory
      //! @param flags    or'd MkDirFlags
      //! @param mode     access mode, or'd AccessMode::Mode
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus MkDir( const std::string &path,
                          uint8_t            flags,
                          uint16_t           mode,
                          uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Remove a directory - async
      //!
      //! @param path     path to the directory to be removed
      //! @param handler  handler to be notified when the response arrives
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RmDir( const std::string &path,
                          ResponseHandler   *handler,
                          uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Remove a directory - sync
      //!
      //! @param path     path to the directory to be removed
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RmDir( const std::string &path,
                          uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Change access mode on a directory or a file - async
      //!
      //! @param path     file/directory path
      //! @param mode     access mode, or'd AccessMode::Mode
      //! @param handler  handler to be notified when the response arrives
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ChMod( const std::string &path,
                          uint16_t           mode,
                          ResponseHandler   *handler,
                          uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Change access mode on a directory or a file - sync
      //!
      //! @param path     file/directory path
      //! @param mode     access mode, or'd AccessMode::Mode
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus ChMod( const std::string &path,
                          uint16_t           mode,
                          uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Check if the server is alive - async
      //!
      //! @param handler  handler to be notified when the response arrives
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Ping( ResponseHandler *handler,
                         uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Check if the server is alive - sync
      //!
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Ping( uint16_t timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain status information for a path - async
      //!
      //! @param path    file/directory path
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a StatInfo object
      //!                if the procedure is successfull
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Stat( const std::string &path,
                         ResponseHandler   *handler,
                         uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain status information for a path - sync
      //!
      //! @param path     file/directory path
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Stat( const std::string  &path,
                         StatInfo          *&response,
                         uint16_t            timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain status information for a Virtual File System - async
      //!
      //! @param path    file/directory path
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a StatInfoVFS object
      //!                if the procedure is successfull
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus StatVFS( const std::string &path,
                            ResponseHandler   *handler,
                            uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain status information for a Virtual File System - sync
      //!
      //! @param path     file/directory path
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus StatVFS( const std::string  &path,
                            StatInfoVFS       *&response,
                            uint16_t            timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain server protocol information - async
      //!
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a ProtocolInfo object
      //!                if the procedure is successfull
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Protocol( ResponseHandler *handler,
                             uint16_t         timeout = 0 );

      //------------------------------------------------------------------------
      //! Obtain server protocol information - sync
      //!
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Protocol( ProtocolInfo *&response,
                             uint16_t       timeout = 0 );

      //------------------------------------------------------------------------
      //! List entries of a directory - async
      //!
      //! @param path    directory path
      //! @param handler handler to be notified when the response arrives,
      //!                the response parameter will hold a DirectoryList
      //!                object if the procedure is successfull
      //! @param timeout timeout value, if 0 the environment default will
      //!                be used
      //! @return        status of the operation
      //------------------------------------------------------------------------
      XRootDStatus DirList( const std::string &path,
                            ResponseHandler   *handler,
                            uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! List entries of a directory - sync
      //!
      //! @param path     directory path
      //! @param mode     DirListFlags
      //! @param response the response (to be deleted by the user)
      //! @param timeout  timeout value, if 0 the environment default will
      //!                 be used
      //! @return         status of the operation
      //------------------------------------------------------------------------
      XRootDStatus DirList( const std::string  &path,
                            uint8_t            flags,
                            DirectoryList    *&response,
                            uint16_t            timeout = 0 );

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

/**
 * This file is part of XrdClHttp
 */

#ifndef __HTTP_FILE_PLUG_IN_
#define __HTTP_FILE_PLUG_IN_

#include "davix.hpp"

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClPlugInInterface.hh"

#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>

// Indicate desire to avoid http "Range: bytes=234-567" header
// Some HTTP(s) data source does not honor Range request, and always start from
// offset 0 when encounter a Range request, for example:
// https://portal.nersc.gov/archive/home/projects/incite11/www/20C_Reanalysis_version_3/everymember_anal_netcdf/daily/WSPD10m/WSPD10m_1808_daily.tar
//
// 1. via Unix env via: this is global, avoid http ranger for all URLs
#define HTTP_FILE_PLUG_IN_AVOIDRANGE_ENV "XRDCLHTTP_AVOIDRANGE"
// 2. via CGI in URl, this only affect the associated URL
#define HTTP_FILE_PLUG_IN_AVOIDRANGE_CGI "xrdclhttp_avoidrange"

namespace XrdCl {

class Log;

class HttpFilePlugIn : public FilePlugIn {
 public:
  HttpFilePlugIn();
  virtual ~HttpFilePlugIn() noexcept;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::Open
  //------------------------------------------------------------------------
  virtual XRootDStatus Open( const std::string &url,
                             OpenFlags::Flags   flags,
                             Access::Mode       mode,
                             ResponseHandler   *handler,
                             uint16_t           timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::Close
  //------------------------------------------------------------------------
  virtual XRootDStatus Close( ResponseHandler *handler,
                              uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::Stat
  //------------------------------------------------------------------------
  virtual XRootDStatus Stat( bool             force,
                             ResponseHandler *handler,
                             uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::Read
  //------------------------------------------------------------------------
  virtual XRootDStatus Read( uint64_t         offset,
                             uint32_t         size,
                             void            *buffer,
                             ResponseHandler *handler,
                             uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::PgRead - async
  //------------------------------------------------------------------------
  virtual XRootDStatus PgRead( uint64_t         offset,
                               uint32_t         size,
                               void            *buffer,
                               ResponseHandler *handler,
                               uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::Write
  //------------------------------------------------------------------------
  virtual XRootDStatus Write( uint64_t         offset,
                              uint32_t         size,
                              const void      *buffer,
                              ResponseHandler *handler,
                              uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::PgWrite - async
  //------------------------------------------------------------------------
  virtual XRootDStatus PgWrite( uint64_t               offset,
                                uint32_t               size,
                                const void            *buffer,
                                std::vector<uint32_t> &cksums,
                                ResponseHandler       *handler,
                                uint16_t               timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::Sync
  //------------------------------------------------------------------------
  virtual XRootDStatus Sync( ResponseHandler *handler,
                             uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::VectorRead
  //------------------------------------------------------------------------
  virtual XRootDStatus VectorRead( const ChunkList &chunks,
                                   void            *buffer,
                                   XrdCl::ResponseHandler *handler,
                                   uint16_t         timeout ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::IsOpen
  //------------------------------------------------------------------------
  virtual bool IsOpen() const override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::SetProperty
  //------------------------------------------------------------------------
  virtual bool SetProperty( const std::string &name,
                            const std::string &value ) override;

  //------------------------------------------------------------------------
  //! @see XrdCl::File::GetProperty
  //------------------------------------------------------------------------
  virtual bool GetProperty( const std::string &name,
                            std::string &value ) const override;

 private:

  Davix::Context *davix_context_;
  Davix::DavPosix *davix_client_;

  DAVIX_FD* davix_fd_;

  std::mutex offset_locker;
  uint64_t curr_offset;

  bool avoid_pread_;
  bool isChannelEncrypted;

  bool is_open_;
  uint64_t filesize;

  std::string url_;

  std::unordered_map<std::string, std::string> properties_;

  Log* logger_;
};

}

#endif // __HTTP_FILE_PLUG_IN_

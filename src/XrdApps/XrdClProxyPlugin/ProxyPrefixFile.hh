//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Elvin Sindrilaru <esindril@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#pragma once
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInInterface.hh"

#include <utility>

using namespace XrdCl;

namespace xrdcl_proxy
{
//------------------------------------------------------------------------------
//! XrdClFile plugin that appends an URL prefix to the given URL. The URL
//! prefix is set as an environment variable XRD_URL_PREFIX.
//------------------------------------------------------------------------------
class ProxyPrefixFile: public XrdCl::FilePlugIn
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  ProxyPrefixFile();

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~ProxyPrefixFile() override;

  //----------------------------------------------------------------------------
  //! Open
  //----------------------------------------------------------------------------
  virtual XRootDStatus Open(const std::string& url,
                            OpenFlags::Flags flags,
                            Access::Mode mode,
                            ResponseHandler* handler,
                            uint16_t timeout) override;

  //----------------------------------------------------------------------------
  //! Close
  //----------------------------------------------------------------------------
  virtual XRootDStatus Close(ResponseHandler* handler,
                             uint16_t         timeout) override
  {
    return pFile->Close(handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Stat
  //----------------------------------------------------------------------------
  virtual XRootDStatus Stat(bool             force,
                            ResponseHandler* handler,
                            uint16_t         timeout) override
  {
    return pFile->Stat(force, handler, timeout);
  }


  //----------------------------------------------------------------------------
  //! Read
  //----------------------------------------------------------------------------
  virtual XRootDStatus Read(uint64_t         offset,
                            uint32_t         size,
                            void*            buffer,
                            ResponseHandler* handler,
                            uint16_t         timeout) override
  {
    return pFile->Read(offset, size, buffer, handler, timeout);
  }

  //------------------------------------------------------------------------
  //! PgRead
  //------------------------------------------------------------------------
  virtual XRootDStatus PgRead( uint64_t         offset,
                               uint32_t         size,
                               void            *buffer,
                               ResponseHandler *handler,
                               uint16_t         timeout ) override
  {
    return pFile->PgRead(offset, size, buffer, handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Write
  //----------------------------------------------------------------------------
  virtual XRootDStatus Write(uint64_t         offset,
                             uint32_t         size,
                             const void*      buffer,
                             ResponseHandler* handler,
                             uint16_t         timeout) override
  {
    return pFile->Write(offset, size, buffer, handler, timeout);
  }

  //------------------------------------------------------------------------
  //! Write
  //------------------------------------------------------------------------
  virtual XRootDStatus Write( uint64_t          offset,
                              Buffer          &&buffer,
                              ResponseHandler  *handler,
                              uint16_t          timeout = 0 ) override
  {
    return pFile->Write(offset, std::move(buffer), handler, timeout);
  }

  //------------------------------------------------------------------------
  //! Write
  //------------------------------------------------------------------------
  virtual XRootDStatus Write( uint64_t            offset,
                              uint32_t            size,
                              Optional<uint64_t>  fdoff,
                              int                 fd,
                              ResponseHandler    *handler,
                              uint16_t            timeout = 0 ) override
  {
    return pFile->Write(offset, size, fdoff, fd, handler, timeout);
  }

  //------------------------------------------------------------------------
  //! PgWrite
  //------------------------------------------------------------------------
  virtual XRootDStatus PgWrite( uint64_t               offset,
                                uint32_t               nbpgs,
                                const void            *buffer,
                                std::vector<uint32_t> &cksums,
                                ResponseHandler       *handler,
                                uint16_t               timeout ) override
  {
    return pFile->PgWrite(offset, nbpgs, buffer, cksums, handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Sync
  //----------------------------------------------------------------------------
  virtual XRootDStatus Sync(ResponseHandler* handler,
                            uint16_t         timeout) override
  {
    return pFile->Sync(handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Truncate
  //----------------------------------------------------------------------------
  virtual XRootDStatus Truncate(uint64_t         size,
                                ResponseHandler* handler,
                                uint16_t         timeout) override
  {
    return pFile->Truncate(size, handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! VectorRead
  //----------------------------------------------------------------------------
  virtual XRootDStatus VectorRead(const ChunkList& chunks,
                                  void*            buffer,
                                  ResponseHandler* handler,
                                  uint16_t         timeout) override
  {
    return pFile->VectorRead(chunks, buffer, handler, timeout);
  }

  //------------------------------------------------------------------------
  //! VectorWrite
  //------------------------------------------------------------------------
  virtual XRootDStatus VectorWrite( const ChunkList &chunks,
                                    ResponseHandler *handler,
                                    uint16_t         timeout = 0 ) override
  {
    return pFile->VectorWrite(chunks, handler, timeout);
  }

  //------------------------------------------------------------------------
  //! @see XrdCl::File::WriteV
  //------------------------------------------------------------------------
  virtual XRootDStatus WriteV( uint64_t            offset,
                               const struct iovec *iov,
                               int                 iovcnt,
                               ResponseHandler    *handler,
                               uint16_t            timeout = 0 ) override
  {
    return pFile->WriteV(offset, iov, iovcnt, handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Fcntl
  //----------------------------------------------------------------------------
  virtual XRootDStatus Fcntl(const Buffer&    arg,
                             ResponseHandler* handler,
                             uint16_t         timeout) override
  {
    return pFile->Fcntl(arg, handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! Visa
  //----------------------------------------------------------------------------
  virtual XRootDStatus Visa(ResponseHandler* handler,
                            uint16_t         timeout) override
  {
    return pFile->Visa(handler, timeout);
  }

  //----------------------------------------------------------------------------
  //! IsOpen
  //----------------------------------------------------------------------------
  virtual bool IsOpen() const override
  {
    return pFile->IsOpen();
  }

  //----------------------------------------------------------------------------
  //! SetProperty
  //----------------------------------------------------------------------------
  virtual bool SetProperty(const std::string& name,
                           const std::string& value) override
  {
    return pFile->SetProperty(name, value);
  }

  //----------------------------------------------------------------------------
  //! GetProperty
  //----------------------------------------------------------------------------
  virtual bool GetProperty(const std::string& name,
                           std::string& value) const override
  {
    return pFile->GetProperty(name, value);
  }

private:

  //----------------------------------------------------------------------------
  //! Trim whitespaces from both ends of a string
  //!
  //! @return trimmed string
  //----------------------------------------------------------------------------
  inline std::string trim(const std::string& in) const;

  //----------------------------------------------------------------------------
  //! Get proxy prefix URL from the environment
  //!
  //! @return proxy prefix RUL
  //----------------------------------------------------------------------------
  inline std::string GetPrefixUrl() const;

  //----------------------------------------------------------------------------
  //! Get list of domains which are NOT to be prefixed
  //!
  //! @return list of excluded domains
  //----------------------------------------------------------------------------
  std::list<std::string> GetExclDomains() const;

  //----------------------------------------------------------------------------
  //! Construct final URL if there is a proxy prefix URL specified and if the
  //! exclusion list is satisfied
  //!
  //! @param orig_url original url
  //!
  //! @return final URL
  //----------------------------------------------------------------------------
  std::string ConstructFinalUrl(const std::string& orig_url) const;

  //----------------------------------------------------------------------------
  //! Get FQDN for specified host
  //!
  //! @param hostname hostname without domain
  //!
  //! @return FQDN
  //----------------------------------------------------------------------------
  std::string GetFqdn(const std::string& hostname) const;

  bool mIsOpen;
  XrdCl::File* pFile;
};

} // namespace xrdcl_proxy

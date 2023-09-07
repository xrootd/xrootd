/**
 * This file is part of XrdClHttp
 */

#include "XrdClHttp/XrdClHttpFilePlugIn.hh"

#include <unistd.h>

#include <cassert>

#include "XrdClHttp/XrdClHttpPlugInUtil.hh"
#include "XrdClHttp/XrdClHttpPosix.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClStatus.hh"

#include "XrdOuc/XrdOucCRC.hh"

namespace {

int MakePosixOpenFlags(XrdCl::OpenFlags::Flags flags) {
  int posix_flags = 0;
  if (flags & XrdCl::OpenFlags::New) {
    posix_flags |= O_CREAT | O_EXCL;
  }
  if (flags & XrdCl::OpenFlags::Delete) {
    posix_flags |= O_CREAT | O_TRUNC;
  }
  if (flags & XrdCl::OpenFlags::Read) {
    posix_flags |= O_RDONLY;
  }
  if (flags & XrdCl::OpenFlags::Write) {
    posix_flags |= O_WRONLY;
  }
  if (flags & XrdCl::OpenFlags::Update) {
    posix_flags |= O_RDWR;
  }
  return posix_flags;
}

}  // namespace

namespace XrdCl {

Davix::Context *root_davix_context_ = NULL;
Davix::DavPosix *root_davix_client_file_ = NULL;

HttpFilePlugIn::HttpFilePlugIn()
    : davix_fd_(nullptr),
      curr_offset(0),
      is_open_(false),
      filesize(0),
      url_(),
      properties_(),
      logger_(DefaultEnv::GetLog()) {
  SetUpLogging(logger_);
  logger_->Debug(kLogXrdClHttp, "HttpFilePlugin constructed.");

  std::string origin = getenv("XRDXROOTD_PROXY")? getenv("XRDXROOTD_PROXY") : "";
  if ( origin.empty() || origin.find("=") == 0) {
      davix_context_ = new Davix::Context();
      davix_client_ = new Davix::DavPosix(davix_context_);
  }
  else {
      if (root_davix_context_ == NULL) {
          root_davix_context_ = new Davix::Context();
          root_davix_client_file_ = new Davix::DavPosix(root_davix_context_);
      }
      davix_context_ = root_davix_context_;
      davix_client_ = root_davix_client_file_;
  }

}

HttpFilePlugIn::~HttpFilePlugIn() noexcept {
    if (root_davix_context_ == NULL) {
        delete davix_client_;
        delete davix_context_;
    }
}

XRootDStatus HttpFilePlugIn::Open(const std::string &url,
                                  OpenFlags::Flags flags, Access::Mode /*mode*/,
                                  ResponseHandler *handler, uint16_t timeout) {
  if (is_open_) {
    logger_->Error(kLogXrdClHttp, "URL %s already open", url.c_str());
    return XRootDStatus(stError, errInvalidOp);
  }

  if (XrdCl::URL(url).GetProtocol().find("https") == 0) 
    isChannelEncrypted = true;
  else
    isChannelEncrypted = false; 

  avoid_pread_ = false;
  if (getenv(HTTP_FILE_PLUG_IN_AVOIDRANGE_ENV) != NULL) 
    avoid_pread_ = true;
  else {
    XrdCl::URL::ParamsMap CGIs = XrdCl::URL(url).GetParams();
    auto search = CGIs.find(HTTP_FILE_PLUG_IN_AVOIDRANGE_CGI);
    if (search != CGIs.end()) 
      avoid_pread_ = true;
  }

  Davix::RequestParams params;
  if (timeout != 0) {
    struct timespec ts = {timeout, 0};
    params.setOperationTimeout(&ts);
  }

  if (flags & (OpenFlags::Write | OpenFlags::Update | OpenFlags::New)) {
    auto full_path = XrdCl::URL(url).GetLocation();
    auto pos = full_path.find_last_of('/');
    auto base_dir =
        pos != std::string::npos ? full_path.substr(0, pos) : full_path;
    auto mkdir_status =
        Posix::MkDir(*davix_client_, base_dir, XrdCl::MkDirFlags::MakePath,
                    XrdCl::Access::None, timeout);
    if (mkdir_status.IsError()) {
      logger_->Error(kLogXrdClHttp,
                    "Could not create parent directories when opening: %s",
                    url.c_str());
      return mkdir_status;
    }
  }

  if (((flags & OpenFlags::Write) || (flags & OpenFlags::Update)) &&
      (flags & OpenFlags::Delete)) {
    auto stat_info = new StatInfo();
    auto status = Posix::Stat(*davix_client_, url, timeout, stat_info);
    if (status.IsOK()) {
      auto unlink_status = Posix::Unlink(*davix_client_, url, timeout);
      if (unlink_status.IsError()) {
        logger_->Error(
            kLogXrdClHttp,
            "Could not delete existing destination file: %s. Error: %s",
            url.c_str(), unlink_status.GetErrorMessage().c_str());
        return unlink_status;
      }
    }
    delete stat_info;
  }
  else if (flags & OpenFlags::Read) {
    auto stat_info = new StatInfo();
    auto status = Posix::Stat(*davix_client_, url, timeout, stat_info);
    if (status.IsOK()) {
      filesize = stat_info->GetSize();
    }
    delete stat_info;
  }

  auto posix_open_flags = MakePosixOpenFlags(flags);

  logger_->Debug(kLogXrdClHttp,
                 "Open: URL: %s, XRootD flags: %d, POSIX flags: %d",
                 url.c_str(), flags, posix_open_flags);

  // res == std::pair<fd, XRootDStatus>
  auto res = Posix::Open(*davix_client_, url, posix_open_flags, timeout);
  if (!res.first) {
    logger_->Error(kLogXrdClHttp, "Could not open: %s, error: %s", url.c_str(),
                   res.second.ToStr().c_str());
    return res.second;
  }

  davix_fd_ = res.first;

  logger_->Debug(kLogXrdClHttp, "Opened: %s", url.c_str());

  is_open_ = true;
  url_ = url;

  auto status = new XRootDStatus();
  handler->HandleResponse(status, nullptr);

  return XRootDStatus();
}

XRootDStatus HttpFilePlugIn::Close(ResponseHandler *handler,
                                   uint16_t /*timeout*/) {
  if (!is_open_) {
    logger_->Error(kLogXrdClHttp,
                   "Cannot close. URL hasn't been previously opened");
    return XRootDStatus(stError, errInvalidOp);
  }

  logger_->Debug(kLogXrdClHttp, "Closing davix fd: %ld", davix_fd_);

  auto status = Posix::Close(*davix_client_, davix_fd_);
  if (status.IsError()) {
    logger_->Error(kLogXrdClHttp, "Could not close davix fd: %ld, error: %s",
                   davix_fd_, status.ToStr().c_str());
    return status;
  }

  is_open_ = false;
  url_.clear();

  handler->HandleResponse(new XRootDStatus(), nullptr);

  return XRootDStatus();
}

XRootDStatus HttpFilePlugIn::Stat(bool /*force*/, ResponseHandler *handler,
                                  uint16_t timeout) {
  if (!is_open_) {
    logger_->Error(kLogXrdClHttp,
                   "Cannot stat. URL hasn't been previously opened");
    return XRootDStatus(stError, errInvalidOp);
  }

  auto stat_info = new StatInfo();
  auto status = Posix::Stat(*davix_client_, url_, timeout, stat_info);
  // A file that is_open_ = true should not retune 400/3011. the only time this
  // happen is a newly created file. Davix doesn't issue a http PUT so this file
  // won't show up for Stat(). Here we fake a response.
  if (status.IsError() && status.code == 400 && status.errNo == 3011) {
    std::ostringstream data;
    data << 140737018595560 << " " << filesize << " " << 33261 << " " << time(NULL);
    stat_info->ParseServerResponse(data.str().c_str());
  }
  else if (status.IsError()) {
    logger_->Error(kLogXrdClHttp, "Stat failed: %s", status.ToStr().c_str());
    return status;
  }

  logger_->Debug(kLogXrdClHttp, "Stat-ed URL: %s", url_.c_str());

  auto obj = new AnyObject();
  obj->Set(stat_info);

  handler->HandleResponse(new XRootDStatus(), obj);

  return XRootDStatus();
}

XRootDStatus HttpFilePlugIn::Read(uint64_t offset, uint32_t size, void *buffer,
                                  ResponseHandler *handler,
                                  uint16_t /*timeout*/) {
  if (!is_open_) {
    logger_->Error(kLogXrdClHttp,
                   "Cannot read. URL hasn't previously been opened");
    return XRootDStatus(stError, errInvalidOp);
  }

  // DavPosix::pread will return -1 if the pread goes beyond the file size
  size = (offset + size > filesize)? filesize - offset : size;
  std::pair<int, XRootDStatus> res;
  if (! avoid_pread_) {
    res = Posix::PRead(*davix_client_, davix_fd_, buffer, size, offset);
  }
  else { 
    offset_locker.lock();
    if (offset == curr_offset) {
      res = Posix::Read(*davix_client_, davix_fd_, buffer, size);
    }
    else {
      res = Posix::PRead(*davix_client_, davix_fd_, buffer, size, offset);
    }
  }

  if (res.second.IsError()) {
    logger_->Error(kLogXrdClHttp, "Could not read URL: %s, error: %s",
                   url_.c_str(), res.second.ToStr().c_str());
    if (avoid_pread_) offset_locker.unlock();
    return res.second;
  }

  int num_bytes_read = res.first;
  curr_offset = offset + num_bytes_read;
  if (avoid_pread_) offset_locker.unlock();

  logger_->Debug(kLogXrdClHttp, "Read %d bytes, at offset %d, from URL: %s",
                 num_bytes_read, offset, url_.c_str());

  auto status = new XRootDStatus();
  auto chunk_info = new ChunkInfo(offset, num_bytes_read, buffer);
  auto obj = new AnyObject();
  obj->Set(chunk_info);
  handler->HandleResponse(status, obj);

  return XRootDStatus();
}

class PgReadSubstitutionHandler : public XrdCl::ResponseHandler {
  private:
    XrdCl::ResponseHandler *realHandler;
    bool isChannelEncrypted;
  public:
  // constructor
  PgReadSubstitutionHandler(XrdCl::ResponseHandler *a, 
                            bool isHttps) : realHandler(a), isChannelEncrypted(isHttps) {}

  // Response Handler
  void HandleResponse(XrdCl::XRootDStatus *status,
                      XrdCl::AnyObject    *rdresp) {

    if( !status->IsOK() )
    { 
      realHandler->HandleResponse( status, rdresp );
      delete this;
      return;
    }

    //using namespace XrdCl;

    ChunkInfo *chunk = 0;
    rdresp->Get(chunk);

    std::vector<uint32_t> cksums;
    if( isChannelEncrypted )
    {
      size_t nbpages = chunk->length / XrdSys::PageSize;
      if( chunk->length % XrdSys::PageSize )
        ++nbpages;
      cksums.reserve( nbpages );

      size_t  size = chunk->length;
      char   *buffer = reinterpret_cast<char*>( chunk->buffer );

      for( size_t pg = 0; pg < nbpages; ++pg )
      {
        size_t pgsize = XrdSys::PageSize;
        if( pgsize > size ) pgsize = size;
        uint32_t crcval = XrdOucCRC::Calc32C( buffer, pgsize );
        cksums.push_back( crcval );
        buffer += pgsize;
        size   -= pgsize;
      }
    }

    PageInfo *pages = new PageInfo(chunk->offset, chunk->length, chunk->buffer, std::move(cksums));
    delete rdresp;
    AnyObject *response = new AnyObject();
    response->Set( pages );
    realHandler->HandleResponse( status, response );

    delete this;
  }
};

XRootDStatus HttpFilePlugIn::PgRead(uint64_t offset, uint32_t size, void *buffer,
                                    ResponseHandler *handler,
                                    uint16_t timeout) {
  ResponseHandler *substitHandler = new PgReadSubstitutionHandler( handler, isChannelEncrypted );
  XRootDStatus st = Read(offset, size, buffer, substitHandler, timeout);
  return st;
}

XRootDStatus HttpFilePlugIn::Write(uint64_t offset, uint32_t size,
                                   const void *buffer, ResponseHandler *handler,
                                   uint16_t timeout) {
  if (!is_open_) {
    logger_->Error(kLogXrdClHttp,
                   "Cannot write. URL hasn't previously been opened");
    return XRootDStatus(stError, errInvalidOp);
  }

  // res == std::pair<int, XRootDStatus>
  auto res =
      Posix::PWrite(*davix_client_, davix_fd_, offset, size, buffer, timeout);
  if (res.second.IsError()) {
    logger_->Error(kLogXrdClHttp, "Could not write URL: %s, error: %s",
                   url_.c_str(), res.second.ToStr().c_str());
    return res.second;
  }
  else
    filesize += res.first;

  logger_->Debug(kLogXrdClHttp, "Wrote %d bytes, at offset %d, to URL: %s",
                 res.first, offset, url_.c_str());

  handler->HandleResponse(new XRootDStatus(), nullptr);

  return XRootDStatus();
}

//------------------------------------------------------------------------
//! @see XrdCl::File::PgWrite
//------------------------------------------------------------------------
XRootDStatus HttpFilePlugIn::PgWrite( uint64_t               offset,
                                      uint32_t               size,
                                      const void            *buffer,
                                      std::vector<uint32_t> &cksums,
                                      ResponseHandler       *handler,
                                      uint16_t               timeout )
{   (void)cksums;
    return Write(offset, size, buffer, handler, timeout);
}

XRootDStatus HttpFilePlugIn::Sync(ResponseHandler *handler, uint16_t timeout) {
  (void)handler;
  (void)timeout;

  logger_->Debug(kLogXrdClHttp, "Sync is a no-op for HTTP.");

  return XRootDStatus();
}


XRootDStatus HttpFilePlugIn::VectorRead(const ChunkList &chunks, void *buffer,
                                        ResponseHandler *handler,
                                        uint16_t /*timeout*/) {
  if (!is_open_) {
    logger_->Error(kLogXrdClHttp,
                   "Cannot read. URL hasn't previously been opened");
    return XRootDStatus(stError, errInvalidOp);
  }

  const auto num_chunks = chunks.size();
  std::vector<Davix::DavIOVecInput> input_vector(num_chunks);
  std::vector<Davix::DavIOVecOuput> output_vector(num_chunks);

  for (size_t i = 0; i < num_chunks; ++i) {
    input_vector[i].diov_offset = chunks[i].offset;
    input_vector[i].diov_size = chunks[i].length;
    input_vector[i].diov_buffer = chunks[i].buffer;
  }

  // res == std::pair<int, XRootDStatus>
  auto res = Posix::PReadVec(*davix_client_, davix_fd_, chunks, buffer);
  if (res.second.IsError()) {
    logger_->Error(kLogXrdClHttp, "Could not vectorRead URL: %s, error: %s",
                   url_.c_str(), res.second.ToStr().c_str());
    return res.second;
  }

  int num_bytes_read = res.first;

  logger_->Debug(kLogXrdClHttp, "VecRead %d bytes, from URL: %s",
                 num_bytes_read, url_.c_str());

  char *output = static_cast<char *>(buffer);
  for (size_t i = 0; i < num_chunks; ++i) {
    std::memcpy(output + input_vector[i].diov_offset,
                output_vector[i].diov_buffer, output_vector[i].diov_size);
  }

  auto status = new XRootDStatus();
  auto read_info = new VectorReadInfo();
  read_info->SetSize(num_bytes_read);
  read_info->GetChunks() = chunks;
  auto obj = new AnyObject();
  obj->Set(read_info);
  handler->HandleResponse(status, obj);

  return XRootDStatus();
}

bool HttpFilePlugIn::IsOpen() const { return is_open_; }

bool HttpFilePlugIn::SetProperty(const std::string &name,
                                 const std::string &value) {
  properties_[name] = value;
  return true;
}

bool HttpFilePlugIn::GetProperty(const std::string &name,
                                 std::string &value) const {
  const auto p = properties_.find(name);
  if (p == std::end(properties_)) {
    return false;
  }

  value = p->second;
  return true;
}

}  // namespace XrdCl

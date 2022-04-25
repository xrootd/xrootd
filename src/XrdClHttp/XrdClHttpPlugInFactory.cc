/**
 * This file is part of XrdClHttp
 */

#include "XrdClHttp/XrdClHttpPlugInFactory.hh"

#include "XrdVersion.hh"

#include "XrdClHttp/XrdClHttpFilePlugIn.hh"
#include "XrdClHttp/XrdClHttpFileSystemPlugIn.hh"

XrdVERSIONINFO(XrdClGetPlugIn, XrdClGetPlugIn)

extern "C"
{
  void *XrdClGetPlugIn( const void* /*arg*/ )
  {
    return static_cast<void*>( new HttpPlugInFactory());
  }
}

HttpPlugInFactory::~HttpPlugInFactory() {
}

XrdCl::FilePlugIn* HttpPlugInFactory::CreateFile( const std::string &/*url*/ ) {
  return new XrdCl::HttpFilePlugIn();
}

XrdCl::FileSystemPlugIn* HttpPlugInFactory::CreateFileSystem( const std::string& url ) {
  return new XrdCl::HttpFileSystemPlugIn(url);
}

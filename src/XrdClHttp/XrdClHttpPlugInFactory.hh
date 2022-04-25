/**
 * This file is part of XrdClHttp
 */

#ifndef __XRD_CL_HTTP__
#define __XRD_CL_HTTP__

#include <XrdCl/XrdClPlugInInterface.hh>

extern "C"
{
  void *XrdClGetPlugIn( const void* /*arg*/ );
}

class HttpPlugInFactory : public XrdCl::PlugInFactory {
  virtual ~HttpPlugInFactory();

  virtual XrdCl::FilePlugIn *CreateFile( const std::string &url ) override;

  virtual XrdCl::FileSystemPlugIn *CreateFileSystem( const std::string &url ) override;
};

#endif // __XRD_CL_HTTP__

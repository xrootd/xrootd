#ifndef __TEST_SETUP_HH__
#define __TEST_SETUP_HH__

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"

#include <cctype>
#include <cstdlib>
#include <string>

namespace XrdClTests {

//------------------------------------------------------------------------------
//! The settings of the test setup: the URLs of the nodes of the cluster and
//! the paths of the test data. xrdcl/cluster.bats starts the cluster and
//! exports each setting <Name> as XRDTEST_<NAME>. A setting has no default,
//! thus a test which runs without the setup fails, rather than use a server
//! of another test.
//------------------------------------------------------------------------------
class TestSetup
{
  public:
    static TestSetup *Get()
    {
      static TestSetup setup;
      return &setup;
    }

    bool GetString( const std::string &name, std::string &value ) const
    {
      std::string var = "XRDTEST_";
      for( char c : name )
        var += std::toupper( static_cast<unsigned char>( c ) );
      const char *v = std::getenv( var.c_str() );
      if( !v )
        return false;
      value = v;
      return true;
    }

    static XrdCl::Log *GetLog()
    {
      return XrdCl::DefaultEnv::GetLog();
    }
};

}

#endif // __TEST_SETUP_HH__

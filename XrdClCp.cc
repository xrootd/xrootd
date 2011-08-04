//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

int main( int argc, char **argv )
{
  using namespace XrdClient;
  Log *log = Log::GetDefaultLog();
  log->SetLevel( Log::DumpMsg );
  log->Error( AppMsg, "%s %s %d", "krowa", "krokodyl", 12 );
  log->Warning( AppMsg, "%s %s %d\n%s\n%d\n%d", "krowa", "krokodyl", 12, "telewizor", 3, 14 );

  Env *env = DefaultEnv::GetDefaultEnv();
  env->ImportString( "krowa", "asd" );
  env->ImportInt( "krokodyl", "sdf" );
  return 0;
}

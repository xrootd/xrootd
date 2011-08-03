//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"

int main( int argc, char **argv )
{
  using namespace XrdClient;
  Log *log = Log::GetDefaultLog();
  log->Error( AppMsg, "%s %s %d", "krowa", "krokodyl", 12 );
  log->Warning( AppMsg, "%s %s %d\n%s\n%d\n%d", "krowa", "krokodyl", 12, "telewizor", 3, 14 );
  return 0;
}

//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdVersion.hh"

#include "TestEnv.hh"

XrdVERSIONINFO( XrdClGetMonitor, MonitorTest );

class MonitorTest: public XrdCl::Monitor
{
  public:
    //--------------------------------------------------------------------------
    // Contructor
    //--------------------------------------------------------------------------
    MonitorTest( const std::string &exec, const std::string &param ):
      pExec( exec ),
      pParam( param ),
      pInitialized(false)
    {
      XrdCl::Log *log = TestEnv::GetLog();
      log->Debug( 2, "Constructed monitoring, exec %s, param %s",
                      exec.c_str(), param.c_str() );
    }

    //--------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------
    virtual ~MonitorTest() {}

    //--------------------------------------------------------------------------
    // Event
    //--------------------------------------------------------------------------
    virtual void Event( EventCode evCode, void *evData )
    {
      XrdCl::Log *log = TestEnv::GetLog();
      switch( evCode )
      {
        //----------------------------------------------------------------------
        // Got an open event
        //----------------------------------------------------------------------
        case EvOpen:
        {
          OpenInfo *i = (OpenInfo*)evData;
          log->Debug( 2, "Successfully opened file %s at %s, size %ld",
                      i->file->GetURL().c_str(), i->dataServer.c_str(),
                      i->fSize );
        }
        default:
        {
        }
      }
    }

  private:
    std::string pExec;
    std::string pParam;
    bool        pInitialized;
};

//------------------------------------------------------------------------------
// C-mangled symbol for dlopen
//------------------------------------------------------------------------------
extern "C"
{
  void *XrdClGetMonitor( const char *exec, const char *param )
  {
    XrdCl::Log *log = TestEnv::GetLog();
    log->Debug( 2, "Constructing monitoring, exec %s, param %s",
                exec, param );
    return new MonitorTest( exec, param );
  }
}

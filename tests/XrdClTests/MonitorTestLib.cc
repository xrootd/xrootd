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
#include "XrdCl/XrdClUtils.hh"
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
      XrdCl::Log *log = XrdClTests::TestEnv::GetLog();
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
      using namespace XrdCl;
      using namespace XrdClTests;

      Log *log = TestEnv::GetLog();
      switch( evCode )
      {
        //----------------------------------------------------------------------
        // Got a connect event
        //----------------------------------------------------------------------
        case EvConnect:
        {
          ConnectInfo *i = (ConnectInfo*)evData;
          std::string timeStarted = Utils::TimeToString( i->sTOD.tv_sec );
          std::string timeDone    = Utils::TimeToString( i->sTOD.tv_sec );
          log->Debug( 2, "Successfully connected to: %s, started: %s, "
                      "finished: %s, authentication: %s, streams: %d",
                      i->server.c_str(), timeStarted.c_str(), timeDone.c_str(),
                      i->auth.empty() ? "none" : i->auth.c_str(),
                      i->streams );
          break;
        }

        //----------------------------------------------------------------------
        // Got a disconnect event
        //----------------------------------------------------------------------
        case EvDisconnect:
        {
          DisconnectInfo *i = (DisconnectInfo*)evData;
          log->Debug( 2, "Disconnected from: %s, bytes sent: %ld, "
                      "bytes received: %ld, connection time: %d, "
                      "disconnection status: %s",
                      i->server.c_str(), i->sBytes, i->rBytes,
                      i->cTime, i->status.ToString().c_str() );
          break;
        }

        //----------------------------------------------------------------------
        // Got an open event
        //----------------------------------------------------------------------
        case EvOpen:
        {
          OpenInfo *i = (OpenInfo*)evData;
          log->Debug( 2, "Successfully opened file %s at %s, size %ld",
                      i->file->GetURL().c_str(), i->dataServer.c_str(),
                      i->fSize );
          break;
        }

        //----------------------------------------------------------------------
        // Got a close event
        //----------------------------------------------------------------------
        case EvClose:
        {
          CloseInfo *i = (CloseInfo*)evData;
          std::string timeOpen   = Utils::TimeToString( i->oTOD.tv_sec );
          std::string timeClosed = Utils::TimeToString( i->cTOD.tv_sec );
          log->Debug( 2, "Closed file %s, opened: %s, closed: %s, status: %s",
                      i->file->GetURL().c_str(), timeOpen.c_str(),
                      timeClosed.c_str(), i->status->ToStr().c_str() );
          log->Debug( 2, "Closed file %s, bytes: read: %ld, readv: %ld, write:"
                      " %ld", i->file->GetURL().c_str(), i->rBytes, i->vBytes,
                      i->wBytes );
          log->Debug( 2, "Closed file %s, count: read: %d, readv: %d/%d, "
                      "write: %d", i->file->GetURL().c_str(), i->rCount,
                      i->vCount, i->vSegs, i->wCount );

          break;
        }

        //----------------------------------------------------------------------
        // Got an error event
        //----------------------------------------------------------------------
        case EvErrIO:
        {
          ErrorInfo *i = (ErrorInfo*)evData;
          std::string op;
          switch( i->opCode )
          {
            case ErrorInfo::ErrOpen:  op = "Open"; break;
            case ErrorInfo::ErrRead:  op = "Read"; break;
            case ErrorInfo::ErrReadV: op = "ReadV"; break;
            case ErrorInfo::ErrWrite: op = "Write"; break;
            case ErrorInfo::ErrUnc:   op = "Unclassified"; break;
          };
          log->Debug( 2, "Operation on file %s encountered an error: %s "
                      "while %s", i->file->GetURL().c_str(),
                      i->status->ToStr().c_str(), op.c_str() );
          break;
        }

        //----------------------------------------------------------------------
        // Got a copy begin event
        //----------------------------------------------------------------------
        case EvCopyBeg:
        {
          CopyBInfo *i = (CopyBInfo*)evData;
          log->Debug( 2, "Copy operation started: origin %s, target: %s ",
                      i->transfer.origin->GetURL().c_str(),
                      i->transfer.target->GetURL().c_str() );
          break;
        }

        //----------------------------------------------------------------------
        // Got a copy end event
        //----------------------------------------------------------------------
        case EvCopyEnd:
        {
          CopyEInfo *i = (CopyEInfo*)evData;
          std::string timeStart = Utils::TimeToString( i->bTOD.tv_sec );
          std::string timeEnd   = Utils::TimeToString( i->eTOD.tv_sec );
          log->Debug( 2, "Copy operation ended: origin: %s, target: %s, "
                      "start time %s, end time: %s, status: %s",
                      i->transfer.origin->GetURL().c_str(),
                      i->transfer.target->GetURL().c_str(),
                      timeStart.c_str(), timeEnd.c_str(),
                      i->status->ToStr().c_str() );
          break;
        }

        //----------------------------------------------------------------------
        // Got a copy end event
        //----------------------------------------------------------------------
        case EvCheckSum:
        {
          CheckSumInfo *i = (CheckSumInfo*)evData;
          log->Debug( 2, "Checksum for transfer: origin: %s, target: %s, "
                      "checksum %s, is ok: %d",
                      i->transfer.origin->GetURL().c_str(),
                      i->transfer.target->GetURL().c_str(),
                      i->cksum.c_str(), (int)i->isOK );
          log->Debug( 2, "Checksum for transfer: origin: %s, target: %s, "
                      "us elapsed at origin %ld, us leapsed at target: %ld",
                      i->transfer.origin->GetURL().c_str(),
                      i->transfer.target->GetURL().c_str(),
                      i->oTime, i->tTime );
          break;
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
    XrdCl::Log *log = XrdClTests::TestEnv::GetLog();
    log->Debug( 2, "Constructing monitoring, exec %s, param %s",
                exec, param ? param : "" );
    return new MonitorTest( exec, param ? param : "" );
  }
}

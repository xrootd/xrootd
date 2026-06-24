#ifndef __XRDHTTPPROMETHEUS_HH__
#define __XRDHTTPPROMETHEUS_HH__
/******************************************************************************/
/*                                                                            */
/*               X r d H t t p P r o m e t h e u s . h h                      */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/******************************************************************************/

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdSysError;

//-----------------------------------------------------------------------------
//! XrdHttpPrometheus: an HTTP external handler that exposes the process-wide
//! XrdMetricsRegistry in the Prometheus text exposition format. It answers
//! GET requests on a configurable path (default /metrics).
//-----------------------------------------------------------------------------

class XrdHttpPrometheus : public XrdHttpExtHandler
{
public:

bool MatchesPath(const char *verb, const char *path) override;

int  ProcessReq(XrdHttpExtReq &req) override;

int  Init(const char *cfgfile) override {return 0;}

     XrdHttpPrometheus(XrdSysError *eDest, const char *confg, const char *parms);
    ~XrdHttpPrometheus();

private:

void Configure(const char *confg);
void PushLoop();

XrdSysError *m_log;
std::string  m_path;       // request path served, e.g. "/metrics"

// Optional Prometheus Pushgateway push (active when m_pushURL is set).
//
std::string  m_pushURL;    // base gateway URL, e.g. http://gw:9091
std::string  m_pushJob   = "xrootd";
std::string  m_pushInst;   // instance label (default: hostname)
int          m_pushEvery = 30;     // seconds between pushes
std::thread             m_pushThread;
std::mutex              m_pushMtx;
std::condition_variable m_pushCV;
std::atomic<bool>       m_stop{false};
};
#endif

#ifndef __XRDMONFORWARD_HH__
#define __XRDMONFORWARD_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d M o n F o r w a r d . h h                         */
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

#include <ctime>
#include <string>

//-----------------------------------------------------------------------------
//! XrdMonForward streams documents as NDJSON (one JSON object per line) over a
//! plain TCP connection to a downstream consumer — a buffering/forwarding
//! frontend such as Logstash (tcp input), Fluentd (in_tcp), Vector (socket
//! source), or a message-broker bridge. It is deliberately dependency-free.
//!
//! The connection is established lazily and re-established after a failure, with
//! a short cool-down so a down consumer is not retried per document. The connect
//! and send both use bounded timeouts so an unresponsive consumer cannot stall
//! the calling (serializer) thread. Documents produced while disconnected are
//! dropped (counted by the caller); durability/buffering is the downstream's job.
//-----------------------------------------------------------------------------

class XrdMonForward
{
public:

//! @param host  destination host (name or IP literal).
//! @param port  destination TCP port.
XrdMonForward(const std::string& host, int port)
             : host(host), port(port), fd(-1), nextTry(0) {}

~XrdMonForward();

//! Send one document, appending a newline. Connects (or reconnects) as needed.
//! @return true if the line was written; false (and sets err) if the document
//!         was dropped because the consumer is unreachable.
bool Send(const std::string& jsonDoc, std::string& err);

private:

bool Connect(std::string& err);
void Drop();

std::string host;
int         port;
int         fd;        // connected socket, or -1
time_t      nextTry;   // earliest time to retry after a failure (cool-down)
};
#endif

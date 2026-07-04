#ifndef __XRDMONSHOVEL_HH__
#define __XRDMONSHOVEL_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d M o n S h o v e l . h h                          */
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
#include <cstdint>
#include <ctime>
#include <string>

//-----------------------------------------------------------------------------
//! XrdMonShovel streams XSHV-framed buffers of encapsulated UDP monitoring
//! datagrams over TCP to a central collector's --tcp-port. It is the shovel
//! sibling of XrdMonForward (which streams NDJSON documents): the connection
//! is lazy with a short reconnect cool-down and bounded connect/send timeouts,
//! but each (re)connect additionally performs the XSHV hello handshake — the
//! optional shared-secret token is sent and the collector's 1-byte verdict is
//! awaited. An explicit rejection (token/version mismatch) is a configuration
//! error, not a network blip, so it backs off much longer. Buffers produced
//! while disconnected are the caller's to spool or drop.
//-----------------------------------------------------------------------------

class XrdMonShovel
{
public:

//! @param host   central collector host (name or IP literal).
//! @param port   central collector TCP port.
//! @param token  shared secret for the hello (may be empty).
XrdMonShovel(std::string host, int port, std::string token)
            : host(std::move(host)), port(port), token(std::move(token)) {}

~XrdMonShovel();

//! Send one framed buffer, connecting (and handshaking) as needed.
//! @return true if the buffer was written; false (and sets err) if the
//!         collector is unreachable or rejected the hello.
bool Send(const std::string& buf, std::string& err);

//! Whether the connection is currently established (for the metrics gauge).
bool Connected() const { return fd.load() >= 0; }

//! Successful connect+handshake count (first connect included).
std::uint64_t Connects() const { return nConnects.load(); }

private:

bool Connect(std::string& err);
void Drop(int cooldown);

std::string      host;
int              port;
std::string      token;
std::atomic<int> fd{-1};       // connected socket, or -1 (read by metrics)
time_t           nextTry = 0;  // earliest reconnect after a failure
std::atomic<std::uint64_t> nConnects{0};
};
#endif

/***************************************************************
 *
 * Copyright (C) 2025, Morgridge Institute for Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef XRDCLCURL_CONNECTIONCALLOUT_HH
#define XRDCLCURL_CONNECTIONCALLOUT_HH

#include <chrono>
#include <string>

namespace XrdClCurl {

class ResponseInfo;

// A class that indicates whether a separate socket creation callout is
// desired for a given request response and, if so, manages the acquisition
// of the new socket.
//
// There are two relevant class methods:
// - `BeginCallout`: Start the process of a socket connection callout.  Returns
//   a file descriptor.  That file descriptor will be listened on; when it is
//   ready, the `FinishCallout` method will be invoked.
// - `FinishCallout`: The listener socket indicates it has waiting data; finish
//   the acquisition of the new socket.
//
// This "listener socket" design is used so a separate thread can generate the
// socket and, when ready, interrupt the thread loop.  If the expiration time passes
// before the socket has a ready read, the object may be deleted.
//
// Finally, the ConnectionCallout is created by setting the property
// `XrdClConnectionCallout` for the relevant `XrdCl::File` or `XrdCl::FileSystem`
// to a string that is the serialized hex value of a function pointer with the
// following signature:
//
// XrdClCurl::ConnectionCallout *CreateCallback(const std::string             &url,
//                                              const XrdClCurl::ResponseInfo &info);
//
// The function is provided with the URL desired and the response information
// leading up to the current request.  It must have C linkage.  If a callout
// is desired, an pointer is returned (the caller owns the memory); otherwise,
// return nullptr to indicate the libcurl default connection logic can be used.
class ConnectionCallout {
public:

    ConnectionCallout() {}
    virtual ~ConnectionCallout() {}
    ConnectionCallout(const ConnectionCallout&) = delete;

    // Start a request to get a socket connection.
    // Returns a FD to monitor for reads on success or -1 on failure.
    // On failure, err is set to the error message.
    //
    // - `err`: On error, set to the human-friendly error message.
    // - `expiration`: The point in time where XrdClCurl will give up on the request and
    //   close the socket.
    virtual int BeginCallout(std::string &err,
        std::chrono::steady_clock::time_point &expiration) = 0;
    
    // Finish the socket connection callout.
    // Invoked when there is data on the file descriptor provided by `BeginCallout`.
    //
    // On success, returns a FD connected to the requested server.
    // Returns -1 on failure and sets err to the error message.
    virtual int FinishCallout(std::string &err) = 0;
};

using CreateConnCalloutType = ConnectionCallout *(*)(const std::string &, const ResponseInfo &);

} // namespace XrdClCurl

#endif // XRDCLCURL_CONNECTIONCALLOUT_HH

//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Authors: Cedric Caffy <ccaffy@cern.ch>, David Smith
// File Date: Aug 2023
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

#ifndef XROOTD_XRDHTTPREADRANGEHANDLER_HH
#define XROOTD_XRDHTTPREADRANGEHANDLER_HH

#include "XrdHttpUtils.hh"
#include <vector>
#include <string>


/**
 * Class responsible for parsing the HTTP Content-Range header
 * coming from the client, generating appropriate read ranges
 * for read or readv and tracking the responses to the requests.
 */
class XrdHttpReadRangeHandler {
public:

  /**
   * These are defaults for:
   * READV_MAXCHUNKS                Max length of the XrdHttpIOList vector.
   * READV_MAXCHUNKSIZE             Max length of a XrdOucIOVec2 element.
   * RREQ_MAXSIZE                   Max bytes to issue in a whole readv/read.
   */
  static constexpr size_t READV_MAXCHUNKS    = 512;
  static constexpr size_t READV_MAXCHUNKSIZE = 512*1024;
  static constexpr size_t RREQ_MAXSIZE       = 8*1024*1024;

  /**
   * Configuration can give specific values for the max chunk
   * size, number of chunks and maximum overall request size,
   * to override the defaults.
   */
  struct Configuration {
    Configuration() : haveSizes(false) { }

    Configuration(const size_t vectorReadMaxChunkSize,
                  const size_t vectorReadMaxChunks,
                  const size_t rRequestMaxBytes)  :
      haveSizes(true), readv_ior_max(vectorReadMaxChunkSize),
      readv_iov_max(vectorReadMaxChunks), reqs_max(rRequestMaxBytes) { }
    

    bool haveSizes;
    size_t readv_ior_max; // max chunk size
    size_t readv_iov_max; // max number of chunks
    size_t reqs_max;      // max bytes in read or readv
  };

  /**
   * Error structure for storing error codes and message.
   * operator bool() can be used to query if a value is set.
   */
  struct Error {
    bool        errSet{false};
    int         httpRetCode{0};
    std::string errMsg;

    explicit operator bool() const { return errSet; }

    void        set(int rc, const std::string &m)
                 { httpRetCode = rc; errMsg = m; errSet = true;     }

    void reset() { httpRetCode = 0; errMsg.clear(); errSet = false; }
  };

  /**
   * Structure for recording or reporting user ranges. The can specify an
   * unbounded range where eiter the start or end offset is not specified.
   */
  struct UserRange {
    bool start_set;
    bool end_set;
    off_t start;
    off_t end;

    UserRange() : start_set(false), end_set(false), start(0), end(0) { }

    UserRange(off_t st, off_t en) : start_set(true), end_set(true), start(st),
                                    end(en) { }
  };

  typedef std::vector<UserRange> UserRangeList;

  /**
   * Constructor.
   * Supplied with an Configuration object. The supplied object remains owned
   * by the caller, but should remain valid throughout the lifetime of the
   * ReadRangeHandler.
   * 
   * @param @conf                   Configuration object.
   */
  XrdHttpReadRangeHandler(const Configuration &conf)
  {
    rRequestMaxBytes_       = RREQ_MAXSIZE;
    vectorReadMaxChunkSize_ = READV_MAXCHUNKSIZE;
    vectorReadMaxChunks_    = READV_MAXCHUNKS;

    if( conf.haveSizes )
    {
      vectorReadMaxChunkSize_ = conf.readv_ior_max;
      vectorReadMaxChunks_    = conf.readv_iov_max;
      rRequestMaxBytes_       = conf.reqs_max;
    }
    reset();
  }

  /**
   * Parses a configuration into a Configuration object.
   * @param @Eroute    Error reporting object
   * @param @parms     Configuration string.
   * @param @cfg       an output Configuration object
   * @return           0 for success, otherwise failure.
   */
  static int    Configure(XrdSysError &Eroute, const char *const parms,
                          Configuration &cfg);
 
  /**
   * getter for the Error object. The object can be inspected with its operator
   * bool() method to indicate an error has happened. Error code and message are
   * available in other members of Error.
   */
  const Error&  getError() const;

  /**
   * Indicates no valid Range header was given and thus the implication is that
   * whole file is required. A range or ranges may be given that cover the whole
   * file but that situation is not detected.
   */
  bool          isFullFile();

  /**
   * Incidcates whether there is a single range, either given by a Range header
   * with single range or implied by having no Range header.
   * Also returns true for an empty file, although there is no range of bytes.
   * @return    true if there is a single range.
   */
  bool          isSingleRange();

  /**
   * Returns a reference of the list of ranges. These are resolved, meaning that
   * if there was no Range header, or it was in the form -N or N-, the file size
   * is used to compute the actual range of bytes that are needed. The list
   * remains owned by the handler and may be invalidated on reset().
   * @return List of ranges in a UserRangeList object.
   *         The returned list may be empty, i.e. for an empty file or if there
   *         is an error. Use getError() to see if there is an error.
   */
  const UserRangeList &ListResolvedRanges();

  /**
   * Requests a XrdHttpIOList (vector of XrdOucIOVec2) that describes the next
   * bytes that need to be fetched from a file. If there is more than one chunk
   * it is size appropriately for a readv request, if there is one request it
   * should be sent as a read request. Therefore the chunks do not necessarily
   * correspond to the ranges the user requested. The caller issue the requests
   * in the order provided and call NotifyReadResult with the ordered results.
   * @return a reference to a XrdHttpIOList. The object remains owned by the
   *         handler. It may be invalided by a new call to NextReadList() or
   *         reset(). The returned list may be empty, which implies no more
   *         reads are needed One can use getError() to see if there is an
   *         error.
   */
  const XrdHttpIOList &NextReadList();

  /**
   * Force the handler to enter error state. Sets a generic error message
   * if there was not already an error.
   */
  void          NotifyError();

  /**
   * Notifies the handler about the arrival of bytes from a read or readv
   * request. The handler tracks the progress of the arriving bytes against
   * the bytes ranges the user requested.
   * @param ret the number of bytes received
   * @param urp a pointer to a pointer of a UserRange object. If urp is not
   *            nullptr, the pointer to a UserRange is returned that describes
   *            the current range associated with the received bytes. The
   *            handler retains ownership of the returned object. reset() of
   *            the handler invalidates the UserRange object.
   * @param start        is an output bool parameter that indicates whether the
   *                     received bytes mark the start of a UserRange.
   * @param allend       is an output bool parameter that indicates whether the
   *                     received bytes mark the end of all the UserRanges
   * @return 0 upon success, -1 if an error happened.
   * One needs to call the getError() method to return the error.
   */
  int           NotifyReadResult(const ssize_t ret,
                                 const UserRange** const urp,
                                 bool &start,
                                 bool &allend);

  /**
   * Parses the Content-Range header value and sets the ranges within the
   * object.
   * @param line the line under the format "bytes=0-19, 25-30"
   * In case the parsing fails any partial results are cleared. There is no
   * error notification as the rest of the request processing should continue
   * in any case.
   */
  void          ParseContentRange(const char* const line);

  /**
   * Resets the object state, ready for handling a new request.
   */
  void          reset();

  /**
   * Notifies of the current file size. This information is required for
   * processing range requests that imply reading to the end or a certain
   * position before the end of a file. It is also used to determine when read
   * or readv need no longer be issued when reading the whole file.
   * Can be called once or more, after reset() but before isSingleRange(),
   * ListResolvedRanges() or NextReadList() methods.
   * @param sz the size of the file
   * @return 0 upon success, -1 if an error happened.
   * One needs to call the getError() method to return the error.
   */
  int           SetFilesize(const off_t sz);

private:
  int    parseOneRange(char* const str);
  int    rangeFig(const char* const s, bool &set, off_t &start);
  void   resolveRanges();
  void   splitRanges();
  void   trimSplit();

  Error  error_;

  UserRangeList rawUserRanges_;

  bool   rangesResolved_;

  UserRangeList resolvedUserRanges_;

  XrdHttpIOList splitRange_;

  // the position in resolvedUserRanges_ corresponding to all the
  // bytes notified via the NotifyReadResult() method
  size_t resolvedRangeIdx_;
  off_t  resolvedRangeOff_;

  // position of the method splitRanges() in within resolvedUserRanges_
  // from where it split ranges into chunks for sending to read/readv
  size_t splitRangeIdx_;
  off_t  splitRangeOff_;

  // the position in splitRange_ corresponding to all the
  // bytes notified via the NotifyReadResult() method
  size_t currSplitRangeIdx_;
  int    currSplitRangeOff_;

  off_t  filesize_;

  size_t vectorReadMaxChunkSize_;
  size_t vectorReadMaxChunks_;
  size_t rRequestMaxBytes_;
};


#endif //XROOTD_XRDHTTPREADRANGEHANDLER_HH

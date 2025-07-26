/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClS3 client plugin for XRootD.                 */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "XrdClS3DownloadHandler.hh"
#include "XrdClS3Factory.hh"
#include "XrdClS3Filesystem.hh"

#include <tinyxml.h>
#include <XrdCl/XrdClURL.hh>
#include <XrdCl/XrdClLog.hh>

#include <charconv>

using namespace XrdClS3;

namespace {

// Helper function to URL-quote a string.
std::string urlquote(const std::string input) {
	std::string output;
	output.reserve(3 * input.size());
	for (char val : input) {
		if ((val >= 48 && val <= 57) ||	 // Digits 0-9
			(val >= 65 && val <= 90) ||	 // Uppercase A-Z
			(val >= 97 && val <= 122) || // Lowercase a-z
			(val == 95 || val == 46 || val == 45 || val == 126 ||
			 val == 47)) // '_.-~/'
		{
			output += val;
		} else {
			output += "%" + std::to_string(val);
		}
	}
	return output;
}

// Helper function for joining two URLs without introducing a double '/'
std::string JoinUrl(const std::string & base, const std::string & path) {
    std::string result = base;
    if (!base.empty() && base[base.size()-1] == '/') {
        size_t idx = 0;
        while (idx < path.size() && path[idx] == '/') idx++;
        result.append(path.data() + idx, path.size() - idx);
    } else {
        result += path;
    }
    return result;
}

class StatHandler : public XrdCl::ResponseHandler {
public:
    StatHandler(const std::string &path, const std::string &s3_url, XrdClCurl::HeaderCallout *header_callout, XrdCl::ResponseHandler *handler, Filesystem::timeout_t timeout, XrdCl::Log &log) :
        m_timeout(timeout),
        m_handler(handler),
        m_header_callout(header_callout),
        m_path(path),
        m_s3_url(s3_url),
        m_logger(log)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

private:
    Filesystem::timeout_t m_timeout;
    XrdCl::ResponseHandler *m_handler{nullptr};
    XrdClCurl::HeaderCallout *m_header_callout{nullptr};
    std::string m_path;
    std::string m_s3_url;
    XrdCl::Log &m_logger;
};

// If the stat request returns a "file not found" error, then there is definitely not
// an object at the given path. However, it could be a directory, so we
// issue a directory listing request to see if it is a directory.
// This is the response handler for that directory listing request.
class StatHandlerDirectory : public XrdCl::ResponseHandler {
public:
    StatHandlerDirectory(XrdCl::ResponseHandler *handler) :
        m_handler(handler)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

private:
    XrdCl::ResponseHandler *m_handler{nullptr};
};

// Response handler for the S3 directory listing GET operation.
class DirListResponseHandler : public XrdCl::ResponseHandler {
public:
    DirListResponseHandler(bool existence_check, const std::string &url, XrdClCurl::HeaderCallout *header_callout, XrdCl::ResponseHandler *handler, time_t expiry, XrdCl::Log &log) :
        m_existence_check(existence_check),
        m_expiry(expiry),
        m_header_callout(header_callout),
        m_url(url),
        m_host(Factory::ExtractHostname(url)),
        m_handler(handler),
        m_logger(log)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

private:
    // Sometimes we are simply looking to see if a "directory" exists; in such a case, we
    // don't need to enumerate all the entries in the bucket and can exit early after the first subdir
    // or file is found
    bool m_existence_check;

    time_t m_expiry; // Expiration time for the directory listing request
    XrdClCurl::HeaderCallout *m_header_callout{nullptr}; // Header callout for S3 signing
    std::string m_url; // The URL of the S3 directory listing
    std::string m_host; // The host address of the S3 endpoint

    std::unique_ptr<XrdCl::DirectoryList> dirlist{new XrdCl::DirectoryList()}; // Directory listing object to hold the results

    XrdCl::ResponseHandler *m_handler{nullptr};
    XrdCl::Log &m_logger;
};

// Handle the creation of a zero-sized file that indicates a "directory"
class MkdirHandler : public XrdCl::ResponseHandler {
public:
    MkdirHandler(XrdCl::File *file, XrdCl::ResponseHandler *handler, Filesystem::timeout_t timeout) :
        m_expiry(time(NULL) + (timeout ? timeout : 30)),
        m_file(file),
        m_handler(handler)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

private:
    time_t m_expiry;
    bool m_started_close{false};
    std::unique_ptr<XrdCl::File> m_file;
    XrdCl::ResponseHandler *m_handler{nullptr};
};

void
StatHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw) {
    std::unique_ptr<StatHandler> self(this);
    std::unique_ptr<XrdCl::AnyObject> response_holder(response_raw);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);

    if (!status) {
        if (m_handler) {return m_handler->HandleResponse(status.release(), response_holder.release());}
        else return;
    }

    if (status->IsOK() || status->errNo != kXR_NotFound) {
        if (m_handler) {return m_handler->HandleResponse(status.release(), response_holder.release());}
        else return;
    }

    // We got a "file not found" type of response.  In this case, we could interpret
    // this as a directory.
    std::string https_url, err_msg;
    const auto s3_url = JoinUrl(m_s3_url, m_path);
    std::string obj;
    if (!Factory::GenerateHttpUrl(s3_url, https_url, &obj, err_msg)) {
        if (m_handler) return m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, err_msg), nullptr);
        else return;
    }
    obj = obj.substr(0, obj.find('?'));
    auto query_loc = https_url.find('?');
    https_url += (query_loc == std::string::npos) ? "?" : "&";
    https_url += "list-type=2&delimiter=/&encoding-type=url";
    https_url += "&prefix=" + urlquote(obj) + "/";

    auto expiry = time(NULL) + m_timeout;

    auto st = DownloadUrl(
        https_url,
        m_header_callout,
        new DirListResponseHandler(
            true, https_url, m_header_callout, new StatHandlerDirectory(m_handler), expiry, m_logger
        ),
        m_timeout
    );
    if (!st.IsOK()) {
        if (m_handler) return m_handler->HandleResponse(new XrdCl::XRootDStatus(st), response_holder.release());
        else return;
    }
}

void
StatHandlerDirectory::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
    std::unique_ptr<StatHandlerDirectory> self(this);
    if (!m_handler) {
        delete response;
        delete status;
        return;
    }
    if (!status || !status->IsOK()) {
        return m_handler->HandleResponse(status, response);
    }
    auto stat_info = new XrdCl::StatInfo("nobody", 0, XrdCl::StatInfo::IsDir, 0);
    auto obj = new XrdCl::AnyObject();
    obj->Set(stat_info);
    delete response;
    m_handler->HandleResponse(status, obj);
}

void
DirListResponseHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw) {
    std::unique_ptr<DirListResponseHandler> self(this);
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    if (!m_handler) {
        return;
    }
    if (!status || !status->IsOK()) {
        return m_handler->HandleResponse(status.release(), response.release());
    }

    if (!response) {
        m_logger.Error(kLogXrdClS3, "Directory listing returned without any response object.");
        return m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidResponse, 0, "No response object provided"), nullptr);
    }

    XrdCl::Buffer *buffer = nullptr;
    response->Get(buffer);
    if (!buffer) {
        m_logger.Error(kLogXrdClS3, "Directory listing response object was not a buffer.");
        return m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidResponse, 0, "No buffer in response object"), nullptr);
    }

    // Parse the XML response from the S3 service
	TiXmlDocument doc;
    std::string buffer_str(buffer->GetBuffer(), buffer->GetSize());
	doc.Parse(buffer_str.c_str());
	if (doc.Error()) {
        std::string errMsg = "Error when parsing S3 endpoint's listing response: " + std::string(doc.ErrorDesc());
		m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidResponse, 0, errMsg), nullptr);
		return;
	}

	auto elem = doc.RootElement();
	if (strcmp(elem->Value(), "ListBucketResult")) {
        m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidResponse, 0, 
            "S3 ListBucket response is not rooted with ListBucketResult element"), nullptr);
		return;
	}

	// Example response from S3:
	// <?xml version="1.0" encoding="utf-8"?>
	// <ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
	//   <Name>genome-browser</Name>
	//   <Prefix>cells/muscle-ibm/endothelial-stromal-cells</Prefix>
	//   <KeyCount>40</KeyCount>
	//   <MaxKeys>40</MaxKeys>
	//   <NextContinuationToken>1PnsptbFFpBSb6UBNN4F/RrxtBvIHjNpdXNYlX8E7IyqXRK26w2y36KViUAbyPPsjzikVY0Zj4jMvQHRhsGWZbcKKrEVvaR0HaZDtfUXUwnc=</NextContinuationToken>
	// <IsTruncated>false</IsTruncated>
	//   <Contents>
	//     <Key>cells/muscle-ibm/endothelial-stromal-cells/UMAP.coords.tsv.gz</Key>
	//     <LastModified>2023-08-21T11:02:53.000Z</LastModified>
	//     <ETag>"b9b0065f10cbd91c9d341acc235c63b0"</ETag>
	//     <Size>360012</Size>
	//     <StorageClass>STANDARD</StorageClass>
	//   </Contents>
	//   <Contents>
	//     <Key>cells/muscle-ibm/endothelial-stromal-cells/barcodes.tsv.gz</Key>
	//     <LastModified>2023-07-17T11:02:19.000Z</LastModified>
	//     <ETag>"048feef5d340e2dd4d2d2d495c24ad7e"</ETag>
	//     <Size>118061</Size>
	//     <StorageClass>STANDARD</StorageClass>
	//   </Contents>
	// ... (truncated some entries for readability) ...
	//   <CommonPrefixes>
	//     <Prefix>cells/muscle-ibm/endothelial-stromal-cells/coords/</Prefix>
	//   </CommonPrefixes>
	//   <CommonPrefixes>
	//     <Prefix>cells/muscle-ibm/endothelial-stromal-cells/markers/</Prefix>
	//   </CommonPrefixes>
	//  <CommonPrefixes>
	//    <Prefix>cells/muscle-ibm/endothelial-stromal-cells/metaFields/</Prefix>
	//  </CommonPrefixes>
	// </ListBucketResult>
	bool isTruncated = false;
    std::string ct;
    bool found_sentinel = false;
	for (auto child = elem->FirstChildElement(); child != nullptr;
		 child = child->NextSiblingElement()) {
		if (!strcmp(child->Value(), "IsTruncated")) {
            auto text = child->GetText();
            if (!strcasecmp(text, "true")) {
                isTruncated = true;
            } else if (!strcasecmp(text, "false")) {
                isTruncated = false;
            }
		} else if (!strcmp(child->Value(), "CommonPrefixes")) {
			auto prefix = child->FirstChildElement("Prefix");
			if (prefix != nullptr) {
				auto prefixChar = prefix->GetText();
				if (prefixChar != nullptr) {
					auto prefixStr = std::string_view(prefixChar);
					Factory::TrimView(prefixStr);
					if (!prefixStr.empty()) {
                        if (prefixStr[prefixStr.size() - 1] == '/') prefixStr = prefixStr.substr(0, prefixStr.size() - 1);
                        uint32_t flags = XrdCl::StatInfo::Flags::IsReadable |
                                        XrdCl::StatInfo::Flags::IsDir |
                                        XrdCl::StatInfo::Flags::XBitSet;
                        dirlist->Add(
                            new XrdCl::DirectoryList::ListEntry(
                                m_host, std::string(prefixStr), new XrdCl::StatInfo(
                                    "nobody", 4096, flags, 0)));
					}
				}
			}
		} else if (!strcmp(child->Value(), "Contents")) {
			std::string_view keyStr;
			int64_t size = -1;
			bool goodSize = false;
			auto key = child->FirstChildElement("Key");
			if (key != nullptr) {
				auto keyChar = key->GetText();
				if (keyChar != nullptr) {
					keyStr = Factory::TrimView(keyChar);
				}
			}
            auto last_slash = keyStr.rfind('/');
            if (last_slash != std::string_view::npos) {
                if (!Factory::GetMkdirSentinel().empty() && (keyStr.substr(last_slash) == Factory::GetMkdirSentinel())) {
                    found_sentinel = true;
                    if (m_existence_check) break;
                    else continue;
                }
            }
			auto sizeElem = child->FirstChildElement("Size");
			if (sizeElem != nullptr) {
                auto sizeChar = sizeElem->GetText();
                if (sizeChar != nullptr && *sizeChar) {
                    auto res = std::from_chars(sizeChar, sizeChar + strlen(sizeChar), size);
                    if (res.ec == std::errc()) {
                        goodSize = true;
                    }
                }
			}
            auto lastModifiedElem = child->FirstChildElement("LastModified");
            time_t lastModified = 0;
            if (lastModifiedElem != nullptr) {
                auto lastModifiedChar = lastModifiedElem->GetText();
                if (lastModifiedChar != nullptr) {
                    struct tm tm;
                    // Example format: "2023-08-21T11:02:53.000Z"
                    if (strptime(lastModifiedChar, "%Y-%m-%dT%H:%M:%S", &tm) != nullptr) {
                        tm.tm_isdst = -1;
                        lastModified = mktime(&tm);
                    }
                }
            }
			if (goodSize && !keyStr.empty()) {
				uint32_t flags = XrdCl::StatInfo::Flags::IsReadable;
                dirlist->Add(
                    new XrdCl::DirectoryList::ListEntry(
                        m_host, std::string(keyStr), new XrdCl::StatInfo(
                            "nobody", size, flags, lastModified)));
			}
		} else if (!strcmp(child->Value(), "NextContinuationToken")) {
			auto ctChar = child->GetText();
			if (ctChar) {
				ct = Factory::TrimView(ctChar);
			}
		}
	}
    // - !isTruncated indicates all object listings have been consumed.
    // - If m_existence_check mode is set, then the caller only cares to know that this is a
    //   directory; as soon as the directory has any "contents", then it officially exists and
    //   we can return.
	if (!isTruncated || (m_existence_check && (dirlist->GetSize() || found_sentinel))) {
        // We interpret an "empty directory" as not existing if there's no sentinel object.
        if (!found_sentinel && !dirlist->GetSize()) {
            m_handler->HandleResponse(
                new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errErrorResponse, kXR_NotFound),
                nullptr
            );
            return;
        }
        auto object = new XrdCl::AnyObject();
        object->Set(dirlist.release());
		m_handler->HandleResponse(
            new XrdCl::XRootDStatus{},
            object
        );
        return;
	}

    auto url = m_url + "&continuation-token=" + urlquote(ct);

    // Calculate the timeout based on the current time and the expiry time
    time_t now = time(NULL);
    if (now >= m_expiry) {
        m_handler->HandleResponse(
            new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Request timed out"),
            nullptr
        );

    }

    auto st = DownloadUrl(url, m_header_callout, this, m_expiry - now);
    if (!st.IsOK()) {
        m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
        return;
    }
}

void
MkdirHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw)
{
    std::unique_ptr<MkdirHandler> self(this);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);

    if (!status || !status->IsOK() || m_started_close) {
        if (m_handler) m_handler->HandleResponse(status.release(), response.release());
        return;
    }

    time_t now = time(NULL);
    if (now >= m_expiry) {
        m_handler->HandleResponse(
            new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Request timed out"),
            nullptr
        );
    }

    self.release();
    m_started_close = true;
    auto st = m_file->Close(this, m_expiry - now);
    if (!st.IsOK()) {
        if (m_handler) m_handler->HandleResponse(status.release(), response.release());
        return;
    }
}


} // namespace

Filesystem::Filesystem(const std::string &url, XrdCl::Log *log) :
    m_logger(log),
    m_url(url)
{
    m_url.SetPath("");
    XrdCl::URL::ParamsMap map;
    m_url.SetParams(map);

    m_logger->Debug(kLogXrdClS3, "S3 filesystem constructed with URL: %s.",
        m_url.GetURL().c_str());
}

Filesystem::~Filesystem() noexcept {}

XrdCl::XRootDStatus
Filesystem::DirList(const std::string          &path,
                    XrdCl::DirListFlags::Flags  flags,
                    XrdCl::ResponseHandler     *handler,
                    timeout_t                   timeout)
{
    std::string https_url, err_msg;
    const auto s3_url = JoinUrl(m_url.GetURL(), path);
    std::string obj;
    if (!Factory::GenerateHttpUrl(s3_url, https_url, &obj, err_msg)) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, err_msg);
    }
    obj = obj.substr(0, obj.find('?'));
    auto query_loc = https_url.find('?');
    https_url += (query_loc == std::string::npos) ? "?" : "&";
    https_url += "list-type=2&delimiter=/&encoding-type=url";
    https_url += "&prefix=" + urlquote(obj) + "/";

    auto expiry = time(NULL) + timeout;

    return DownloadUrl(
        https_url,
        &m_header_callout,
        new DirListResponseHandler(
            false, https_url, &m_header_callout, handler, expiry, *m_logger
        ), 
        timeout
    );
}

std::pair<XrdCl::XRootDStatus, XrdCl::FileSystem*>
Filesystem::GetFSHandle(const std::string &path) {
    const auto s3_url = JoinUrl(m_url.GetURL(), path);
    std::string https_url, err_msg;
    if (!Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg)) {
        return std::make_pair(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, err_msg), nullptr);
    }
    auto loc = https_url.find('/', 8); // strlen("https://") -> 8
    if (loc == std::string::npos) {
        return std::make_pair(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, "Invalid generated URL"), nullptr);
    }
    auto endpoint = https_url.substr(0, loc);
    {
        std::shared_lock lock(m_handles_mutex);
        auto iter = m_handles.find(endpoint);
        if (iter != m_handles.end()) {
            return std::make_pair(XrdCl::XRootDStatus{}, iter->second);
        }
    }
    XrdCl::URL url;
    if (!url.FromString(https_url)) {
        return std::make_pair(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, "Invalid generated XrdCl URL"), nullptr);
    }
    std::unique_lock lock(m_handles_mutex);
    auto iter = m_handles.find(endpoint);
    if (iter != m_handles.end()) {
        return std::make_pair(XrdCl::XRootDStatus{}, iter->second);
    }
    auto fs = new XrdCl::FileSystem(url);
    std::stringstream ss;
    ss << std::hex << reinterpret_cast<long long>(&m_header_callout);
    if (!fs->SetProperty("XrdClCurlHeaderCallout", ss.str())) {
        delete fs;
        return std::make_pair(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, "Failed to setup header callout"), nullptr);
    }
    m_handles[endpoint] = fs;

    return std::make_pair(XrdCl::XRootDStatus{}, fs);
}

bool
Filesystem::GetProperty(const std::string &name,
                        std::string       &value) const
{
    std::unique_lock lock(m_properties_mutex);
    const auto p = m_properties.find(name);
    if (p == std::end(m_properties)) {
        return false;
    }

    value = p->second;
    return true;
}

XrdCl::XRootDStatus
Filesystem::Locate(const std::string        &path,
                   XrdCl::OpenFlags::Flags   flags,
                   XrdCl::ResponseHandler   *handler,
                   timeout_t                 timeout)
{
    auto cleaned_path = Factory::CleanObjectName(path);
    auto [st, fs] = GetFSHandle(cleaned_path);
    if (!st.IsOK()) {
        return st;
    }
    return fs->Locate(cleaned_path, flags, handler, timeout);
}

XrdCl::XRootDStatus
Filesystem::MkDir(const std::string        &input_path,
                  XrdCl::MkDirFlags::Flags  flags,
                  XrdCl::Access::Mode       mode,
                  XrdCl::ResponseHandler   *handler,
                  timeout_t                 timeout)
{
    auto sentinel = Factory::GetMkdirSentinel();
    if (sentinel.empty()) {
        if (handler) handler->HandleResponse(new XrdCl::XRootDStatus{}, nullptr);
        return {};
    }
    auto loc = input_path.find('?');
    auto path = input_path.substr(0, loc);
    if (!path.empty() && path[path.size() - 1] != '/') path += "/";
    path += sentinel;
    if (loc != std::string::npos) {
        path += input_path.substr(loc);
    }

    // Try creating a zero-sized sentinel.
    std::string https_url, err_msg;
    const auto s3_url = JoinUrl(m_url.GetURL(), path);
    if (!Factory::GenerateHttpUrl(s3_url, https_url, nullptr, err_msg)) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, err_msg);
    }

    XrdCl::File *http_file(new XrdCl::File());
    auto status = http_file->Open(https_url, XrdCl::OpenFlags::Compress, XrdCl::Access::None, nullptr, Filesystem::timeout_t(0));
    if (!status.IsOK()) {
        delete http_file;
        return status;
    }

    auto callout_loc = reinterpret_cast<long long>(&m_header_callout);
    size_t buf_size = 16;
    char callout_buf[buf_size];
    std::to_chars_result result = std::to_chars(callout_buf, callout_buf + buf_size - 1, callout_loc, 16);
    if (result.ec == std::errc{}) {
        std::string callout_str(callout_buf, result.ptr - callout_buf);
        http_file->SetProperty("XrdClCurlHeaderCallout", callout_str);
    }

    MkdirHandler *mkdirHandler = new MkdirHandler(http_file, handler, timeout);

    return http_file->Open(https_url, XrdCl::OpenFlags::Write, XrdCl::Access::None, mkdirHandler, timeout);
}

XrdCl::XRootDStatus
Filesystem::Query(XrdCl::QueryCode::Code  queryCode,
                  const XrdCl::Buffer     &arg,
                  XrdCl::ResponseHandler  *handler,
                  timeout_t                timeout)
{
    if (queryCode != XrdCl::QueryCode::Checksum && queryCode != XrdCl::QueryCode::XAttr) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errNotImplemented);
    }
    auto cleaned_path = Factory::CleanObjectName(arg.ToString());
    auto [st, fs] = GetFSHandle(cleaned_path);
    if (!st.IsOK()) {
        return st;
    }
    XrdCl::Buffer cleanedArg;
    cleanedArg.FromString(cleaned_path);
    return fs->Query(queryCode, cleanedArg, handler, timeout);
}


XrdCl::XRootDStatus
Filesystem::Rm(const std::string      &path,
               XrdCl::ResponseHandler *handler,
               timeout_t               timeout)
{
    auto cleaned_path = Factory::CleanObjectName(path);
    auto [st, fs] = GetFSHandle(cleaned_path);
    if (!st.IsOK()) {
        return st;
    }
    return fs->Rm(cleaned_path, handler, timeout);
}

XrdCl::XRootDStatus
Filesystem::RmDir(const std::string      &input_path,
                  XrdCl::ResponseHandler *handler,
                  timeout_t               timeout)
{
    auto sentinel = Factory::GetMkdirSentinel();
    if (sentinel.empty()) {
        if (handler) handler->HandleResponse(new XrdCl::XRootDStatus{}, nullptr);
        return {};
    }
    auto loc = input_path.find('?');
    auto path = input_path.substr(0, loc);
    if (!path.empty() && path[path.size() - 1] != '/') path += "/";
    path += sentinel;
    if (loc != std::string::npos) {
        path += input_path.substr(loc);
    }
    return Rm(path, handler, timeout);
}


bool
Filesystem::SetProperty(const std::string &name,
                        const std::string &value)
{
    std::unique_lock lock(m_properties_mutex);
    m_properties[name] = value;
    return true;
}

XrdCl::XRootDStatus
Filesystem::Stat(const std::string      &path,
                 XrdCl::ResponseHandler *handler,
                 timeout_t               timeout)
{
    auto cleaned_path = Factory::CleanObjectName(path);
    auto [st, fs] = GetFSHandle(cleaned_path);
    if (!st.IsOK()) {
        return st;
    }
    return fs->Stat(cleaned_path, new StatHandler(cleaned_path, m_url.GetURL(), &m_header_callout, handler, timeout, *m_logger), timeout);
}

std::shared_ptr<XrdClCurl::HeaderCallout::HeaderList>
Filesystem::S3HeaderCallout::GetHeaders(const std::string &verb,
                                        const std::string &url,
                                        const XrdClCurl::HeaderCallout::HeaderList &headers)
{
    std::string auth_token, err_msg;
    std::shared_ptr<HeaderList> header_list(new HeaderList(headers));
    if (Factory::GenerateV4Signature(url, verb, *header_list, auth_token, err_msg)) {
        header_list->emplace_back("Authorization", auth_token);
    } else {
        m_parent.m_logger->Error(kLogXrdClS3, "Failed to generate V4 signature: %s", err_msg.c_str());
        return nullptr;
    }
    return header_list;
}

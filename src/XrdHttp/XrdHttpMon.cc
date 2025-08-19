#include "XrdHttpMon.hh"

#include <iostream>
#include <sstream>
#include <thread>

#include "XrdSys/XrdSysError.hh"
#include "XrdXrootd/XrdXrootdGStream.hh"

XrdSysError eDest(0, "HttpMon");

typedef std::array<std::array<XrdHttpMon::HttpInfo, XrdHttpMon::StatusCodes::sc_Count>, XrdHttpReq::ReqType::rtCount>
    StatsMatrix;

StatsMatrix XrdHttpMon::statsInfo{};
XrdXrootdGStream* XrdHttpMon::gStream = nullptr;
XrdSysLogger* XrdHttpMon::logger = nullptr;
bool XrdHttpMon::Initialize(XrdSysLogger *logP, XrdXrootdGStream *gStreamP) {
    if (!gStreamP || !logP) {
        return false;
    }
    gStream = gStreamP;
    logger = logP;
    eDest.logger(logP);
    return true;
}

bool XrdHttpMon::IsInitialized() {
    return gStream != nullptr && logger != nullptr;
}

void XrdHttpMon::Report() {
    std::string json = GetMonitoringJson();
    if (!gStream->Insert(json.c_str(), json.size() + 1)) {
        eDest.Emsg("HttpMon", "Gstream Buffer Rejected");
    }
}

void* XrdHttpMon::Start(void*) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Report();
    }
}

void XrdHttpMon::RecordCount(XrdHttpReq::ReqType op, StatusCodes sc) {
    if (op >= XrdHttpReq::ReqType::rtCount || sc >= StatusCodes::sc_Count) return;

    auto& info = statsInfo[op][sc];
    info.count++;
    // info.duration.fetch_add(latency.count(), std::memory_order_relaxed);
}

void XrdHttpMon::RecordError(XrdHttpReq::ReqType op, StatusCodes sc) {
    if (op >= XrdHttpReq::ReqType::rtCount || sc >= StatusCodes::sc_Count) return;

    auto& info = statsInfo[op][sc];
    info.error++;
}

//  This creates a json with the following format:
//  http_GET_200 = {count: <>, bytes: <>, duration: <>}
std::string XrdHttpMon::GetMonitoringJson() {
    std::ostringstream oss;
    oss << "{";

    bool first = true;
    for (size_t op = 0; op < XrdHttpReq::ReqType::rtCount; ++op) {
        std::string opName = GetOperationString(static_cast<XrdHttpReq::ReqType>(op));
        for (size_t sc = 0; sc < StatusCodes::sc_Count; ++sc) {
            auto& info = statsInfo[op][sc];

            uint64_t total_count = info.count;
            uint64_t error_count = info.error;
            // auto duration = info.duration.exchange(0, std::memory_order_relaxed);

            if (total_count == 0) continue;
            std::string key = "HTTP_" + opName + "_" + GetStatusCodeString(static_cast<StatusCodes>(sc));

            if (!first) oss << ",";
            first = false;

            oss << "\"" << key << "\":{";
            oss << "\"count\":" << total_count << ",";
            oss << "\"errors\":" << error_count;
            // double durationSec = 0.0;
            // if (duration > 0) {
            //     durationSec = std::chrono::duration<double>(std::chrono::system_clock::duration(duration)).count();
            // }
            // oss << "\"duration\":" << durationSec;
            oss << "}";
        }
    }

    oss << "}";
    return oss.str();
}

std::string XrdHttpMon::GetOperationString(XrdHttpReq::ReqType op) {
    switch (op) {
        case XrdHttpReq::ReqType::rtDELETE:
            return "DELETE";
        case XrdHttpReq::ReqType::rtHEAD:
            return "HEAD";
        case XrdHttpReq::ReqType::rtGET:
            return "GET";
        case XrdHttpReq::ReqType::rtMKCOL:
            return "MKCOL";
        case XrdHttpReq::ReqType::rtMOVE:
            return "MOVE";
        case XrdHttpReq::ReqType::rtOPTIONS:
            return "OPTIONS";
        case XrdHttpReq::ReqType::rtPROPFIND:
            return "PROPFIND";
        case XrdHttpReq::ReqType::rtPUT:
            return "PUT";
        case XrdHttpReq::ReqType::rtMalformed:
            return "Malformed";
        default:
            return "UNKNOWN";
    }
}

std::string XrdHttpMon::GetStatusCodeString(StatusCodes sc) {
    switch (sc) {
        case sc_100:
            return "100";
        case sc_200:
            return "200";
        case sc_201:
            return "201";
        case sc_202:
            return "202";
        case sc_206:
            return "206";
        case sc_207:
            return "207";
        case sc_302:
            return "302";
        case sc_307:
            return "307";
        case sc_400:
            return "400";
        case sc_401:
            return "401";
        case sc_403:
            return "403";
        case sc_404:
            return "404";
        case sc_405:
            return "405";
        case sc_409:
            return "409";
        case sc_416:
            return "416";
        case sc_423:
            return "423";
        case sc_500:
            return "500";
        case sc_502:
            return "502";
        case sc_504:
            return "504";
        case sc_507:
            return "507";
        default:
            return "UNKNOWN";
    }
}

XrdHttpMon::StatusCodes XrdHttpMon::ToStatusCode(int code) {
    switch (code) {
        case 100:
            return sc_100;
        case 200:
            return sc_200;
        case 201:
            return sc_201;
        case 202:
            return sc_202;
        case 206:
            return sc_206;
        case 207:
            return sc_207;
        case 302:
            return sc_302;
        case 307:
            return sc_307;
        case 400:
            return sc_400;
        case 401:
            return sc_401;
        case 403:
            return sc_403;
        case 404:
            return sc_404;
        case 405:
            return sc_405;
        case 409:
            return sc_409;
        case 416:
            return sc_416;
        case 423:
            return sc_423;
        case 500:
            return sc_500;
        case 502:
            return sc_502;
        case 504:
            return sc_504;
        case 507:
            return sc_507;
        default:
            return sc_UNKNOWN;
    }
}
#include "Xrd/XrdMonRoll.hh"
#include "XrdHttpReq.hh"
#include "XrdSys/XrdSysRAtomic.hh"

#include <array>
#include <chrono>
#include <string>
#include <vector>

class XrdXrootdGStream;
class XrdSysLogger;

class XrdHttpMon {
  public:
    // Supported HTTP status codes
    // We use this separate enum from the one defined in XrdHttpUtils to optimise on space and array indexing, this is
    // not possible with the anonymous enum in httputils mapped to actual status code values making it sparse
    enum StatusCodes {
        sc_100,
        sc_200,
        sc_201,
        sc_202,
        sc_206,
        sc_207,
        sc_302,
        sc_307,
        sc_400,
        sc_401,
        sc_403,
        sc_404,
        sc_405,
        sc_409,
        sc_416,
        sc_423,
        sc_500,
        sc_502,
        sc_504,
        sc_507,
        sc_UNKNOWN,
        sc_Count
    };

    // Per (operation, status code) statistics
    struct HttpInfo { // All fields are cumulative; a reset to 0 usually means a server restart and the monitoring
                      // endpoint shall simply add the values from there on
      RAtomic_uint64_t count{0};         // total http requests
      RAtomic_uint64_t error_network{0}; // errors with the connection
      RAtomic_uint64_t error_xrootd{0};  // errors returned by the protocol/bridge layer
      RAtomic_uint64_t success{0};       // success in responding back with a http response code
      RAtomic_uint64_t duration_us{0};   // sum of operation duration in microseconds
    };

    static void Initialize(XrdSysLogger* logP, XrdXrootdGStream* gStream, XrdMonRoll *mrollP);
    static void* Start(void*);
    static void Record(XrdHttpReq &req, int code);
    static bool IsInitialized() { return isInitialized; }

  private:
    // Global stats table
    static std::array<std::array<HttpInfo, StatusCodes::sc_Count>, XrdHttpReq::ReqType::rtCount> statsInfo;

    static RAtomic_uint64_t verbCounters[XrdHttpReq::ReqType::rtCount];
    static RAtomic_uint64_t statusCounters[StatusCodes::sc_Count];
    static std::vector<XrdMonRoll::Item> statsSchema;

    // Static members for monitoring configuration
    static XrdXrootdGStream* gStream;
    static XrdMonRoll* mrollP;
    static std::chrono::seconds flushPeriod;

    // Flags to track which monitoring types are enabled
    static bool hasGStream;  // True if gStream monitoring is enabled (needs detailed metrics)
    static bool hasMonRoll;  // True if MonRoll summary monitoring is enabled (needs simple counters)
    static bool isInitialized;  // True if monitoring has been initialized

    static void RecordErrProt(XrdHttpReq::ReqType op, StatusCodes sc, std::chrono::steady_clock::duration duration);
    static void RecordErrNet(XrdHttpReq::ReqType op, StatusCodes sc, std::chrono::steady_clock::duration duration);
    static void RecordCount(XrdHttpReq::ReqType op, StatusCodes sc);
    static void RecordSuccess(XrdHttpReq::ReqType op, StatusCodes sc, std::chrono::steady_clock::duration duration);

    // Helper functions for conditional monitoring updates
    // These are simple functions that the compiler will hopefully inline
    static inline void RecordGStreamCount(XrdHttpReq::ReqType op, StatusCodes sc) {
        if (hasGStream) RecordCount(op, sc);
    }
    static inline void RecordGStreamSuccess(XrdHttpReq::ReqType op, StatusCodes sc, std::chrono::steady_clock::duration duration) {
        if (hasGStream) RecordSuccess(op, sc, duration);
    }
    static inline void RecordGStreamErrNet(XrdHttpReq::ReqType op, StatusCodes sc, std::chrono::steady_clock::duration duration) {
        if (hasGStream) RecordErrNet(op, sc, duration);
    }
    static inline void RecordGStreamErrProt(XrdHttpReq::ReqType op, StatusCodes sc, std::chrono::steady_clock::duration duration) {
        if (hasGStream) RecordErrProt(op, sc, duration);
    }
    static inline void RecordMonRollVerb(XrdHttpReq::ReqType op) {
        if (hasMonRoll) verbCounters[op]++;

    }
    static inline void RecordMonRollStatus(StatusCodes sc) {
        if (hasMonRoll) statusCounters[sc]++;
    }

    static StatusCodes ToStatusCode(int code);

    static std::string GetMonitoringJson();

    static std::string GetOperationString(XrdHttpReq::ReqType op);
    static std::string GetStatusCodeString(StatusCodes sc);
    static void Report();

    // Prevent instantiation
    XrdHttpMon() = delete;
    ~XrdHttpMon() = delete;

};

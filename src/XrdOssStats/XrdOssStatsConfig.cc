
#include "XrdVersion.hh"

#include "XrdOssStatsConfig.hh"
#include "XrdOssStatsFileSystem.hh"
#include "XrdSys/XrdSysError.hh"

#include <sstream>

XrdVERSIONINFO(XrdOssGetFileSystem, Stats);


std::string LogMaskToString(int mask) {
    if (mask == LogMask::All) {return "all";}

    bool has_entry = false;
    std::stringstream ss;
    if (mask & LogMask::Debug) {
        ss << "debug";
        has_entry = true;
    }
    if (mask & LogMask::Info) {
        ss << (has_entry ? ", " : "") << "info";
        has_entry = true;
    }
    if (mask & LogMask::Warning) {
        ss << (has_entry ? ", " : "") << "warning";
        has_entry = true;
    }
    if (mask & LogMask::Error) {
        ss << (has_entry ? ", " : "") << "error";
        has_entry = true;
    }
    return ss.str();
}

// Parse a string as a timeout value with a unit.
//
// Example:
//    1s500ms
bool ParseDuration(const std::string &duration, std::chrono::steady_clock::duration &result, std::string &errmsg) {

    if (duration.empty()) {
        errmsg = "cannot parse empty string as a time duration";
        return false;
    }
    if (duration == "0") {
        result = std::chrono::steady_clock::duration(0);
        return true;
    }
    std::chrono::steady_clock::duration dur(0);
    auto strValue = duration;
    while (!strValue.empty()) {
        std::size_t pos;
        double value;
        try {
            value = std::stod(strValue, &pos);
        } catch (std::invalid_argument const &exc) {
            errmsg = "Invalid number provided as timeout: " + strValue;
            return false;
        } catch (std::out_of_range const &exc) {
            errmsg = "Provided timeout out of representable range: " + std::string(exc.what());
            return false;
        }
        if (value < 0) {
            errmsg = "Provided timeout was negative";
            return false;
        }
        strValue = strValue.substr(pos);
        char unit[3] = {'\0', '\0', '\0'};
        if (!strValue.empty()) {
            unit[0] = strValue[0];
            if (unit[0] >= '0' && unit[0] <= '9') {unit[0] = '\0';}
        }
        if (strValue.size() > 1) {
            unit[1] = strValue[1];
            if (unit[1] >= '0' && unit[1] <= '9') {unit[1] = '\0';}
        }
        if (!strncmp(unit, "ns", 2)) {
            dur += std::chrono::duration_cast<typeof(dur)>(std::chrono::duration<double, std::nano>(value));
        } else if (!strncmp(unit, "us", 2)) {
            dur += std::chrono::duration_cast<typeof(dur)>(std::chrono::duration<double, std::micro>(value));
        } else if (!strncmp(unit, "ms", 2)) {
            dur += std::chrono::duration_cast<typeof(dur)>(std::chrono::duration<double, std::milli>(value));
        } else if (!strncmp(unit, "s", 1)) {
            dur += std::chrono::duration_cast<typeof(dur)>(std::chrono::duration<double>(value));
        } else if (!strncmp(unit, "m", 1)) {
            dur += std::chrono::duration_cast<typeof(dur)>(std::chrono::duration<double, std::ratio<60>>(value));
        } else if (!strncmp(unit, "h", 1)) {
            dur += std::chrono::duration_cast<typeof(dur)>(std::chrono::duration<double, std::ratio<3600>>(value));
        } else if (strlen(unit) > 0) {
            errmsg = "Unknown unit in duration: " + std::string(unit);
            return false;
        } else {
            errmsg = "Unit missing from duration: " + duration;
            return false;
        }
        strValue = strValue.substr(strlen(unit));
    }
    result = dur;
    return true;
}

///
// The following functions export the plugin to the
// XRootD framework

extern "C" {

XrdOss *XrdOssAddStorageSystem2(XrdOss       *curr_oss,
                                XrdSysLogger *logger,
                                const char   *config_fn,
                                const char   *parms,
                                XrdOucEnv    *envP)
{
                   
    XrdSysError log(logger, "fsstats_");
    try {
        return new StatsFileSystem(curr_oss, logger, config_fn, envP);
    } catch (std::runtime_error &re) {
        log.Emsg("Initialize", "Encountered a runtime failure:", re.what());
        return nullptr;
    }
}

XrdVERSIONINFO(XrdOssAddStorageSystem2,fsstats);

}

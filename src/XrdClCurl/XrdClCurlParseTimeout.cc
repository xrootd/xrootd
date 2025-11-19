/****************************************************************
 *
 * Copyright (C) 2024, Pelican Project, Morgridge Institute for Research
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

#include "XrdClCurlParseTimeout.hh"

#include <stdexcept>
#include <cstring>

bool XrdClCurl::ParseTimeout(const std::string &duration, struct timespec &result, std::string &errmsg) {

    if (duration.empty()) {
        errmsg = "cannot parse empty string as a time duration";
        return false;
    }
    if (duration == "0") {
        result = {0, 0};
        return true;
    }
    struct timespec ts = {0, 0};
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
            ts.tv_nsec += value;
        } else if (!strncmp(unit, "us", 2)) {
            auto value_s = (static_cast<long long>(value)) / 1'000'000;
            ts.tv_sec += value_s;
            value -= value_s * 1'000'000;
            ts.tv_nsec += value * 1'000'000;
        } else if (!strncmp(unit, "ms", 2)) {
            auto value_s = (static_cast<long long>(value)) / 1'000;
            ts.tv_sec += value_s;
            value -= value_s * 1'000;
            ts.tv_nsec += value * 1'000'000;
        } else if (!strncmp(unit, "s", 1)) {
            auto value_s = (static_cast<long long>(value));
            ts.tv_sec += value_s;
            value -= value_s;
            ts.tv_nsec += value * 1'000'000'000;
        } else if (!strncmp(unit, "m", 1)) {
            value *= 60;
            auto value_s = (static_cast<long long>(value));
            ts.tv_sec += value_s;
            value -= value_s;
            ts.tv_nsec += value * 1'000'000'000;
        } else if (!strncmp(unit, "h", 1)) {
            value *= 3600;
            auto value_s = (static_cast<long long>(value));
            ts.tv_sec += value_s;
            value -= value_s;
            ts.tv_nsec += value * 1'000'000'000;
        } else if (strlen(unit) > 0) {
            errmsg = "Unknown unit in duration: " + std::string(unit);
            return false;
        } else {
            errmsg = "Unit missing from duration: " + duration;
            return false;
        }
        if (ts.tv_nsec > 1'000'000'000) {
            ts.tv_sec += ts.tv_nsec / 1'000'000'000;
            ts.tv_nsec = ts.tv_nsec % 1'000'000'000;
        }
        strValue = strValue.substr(strlen(unit));
    }
    result.tv_nsec = ts.tv_nsec;
    result.tv_sec = ts.tv_sec;
    return true;
}

std::string XrdClCurl::MarshalDuration(const struct timespec &duration)
{
   if (duration.tv_sec == 0 && duration.tv_nsec == 0) {return "0s";}

   std::string result = duration.tv_sec != 0 ? std::to_string(duration.tv_sec) + "s" : "";
   if (duration.tv_nsec) {
       result += std::to_string(duration.tv_nsec / 1'000'000) + "ms";
   }
   return result;
}

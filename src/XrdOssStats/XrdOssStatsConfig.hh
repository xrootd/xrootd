
#pragma once

#include <chrono>
#include <string>

enum LogMask {
    Debug =   0x01,
    Info =    0x02,
    Warning = 0x04,
    Error =   0x08,
    All =     0xff
};

std::string LogMaskToString(int mask);

bool ParseDuration(const std::string &duration, std::chrono::steady_clock::duration &result, std::string &errmsg);

/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClCurl client plugin for XRootD.               */
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

#ifndef XRDCLCURLCHECKSUM_HH_
#define XRDCLCURLCHECKSUM_HH_

#include <array>
#include <string>
#include <tuple>

namespace XrdClCurl {

constexpr size_t g_max_checksum_length = 32;

// Checksum types known to this cache
enum class ChecksumType {
    kCRC32C,
    kMD5,
    kSHA1,
    kSHA256,
    kAll,     // Short-hand for setting all checksums at once.
    kUnknown, // Indicates an unset value; `kUnknown - 1` is used in for-loops to iterate through all checksums.
};

inline const std::string GetTypeString(ChecksumType ctype) {
    switch (ctype) {
        case ChecksumType::kCRC32C:
            return "crc32c";
        case ChecksumType::kMD5:
            return "md5";
        case ChecksumType::kSHA1:
            return "sha1";
        case ChecksumType::kSHA256:
            return "sha256";
        case ChecksumType::kAll: // fallthrough
        case ChecksumType::kUnknown:
            return "unknown";
    }
    return "unknown";
}

inline size_t GetChecksumLength(ChecksumType ctype) {
    switch (ctype) {
        case ChecksumType::kCRC32C:
            return 4;
        case ChecksumType::kMD5:
            return 16;
        case ChecksumType::kSHA1:
            return 20;
        case ChecksumType::kSHA256:
            return 32;
        case ChecksumType::kAll: // fallthrough
        case ChecksumType::kUnknown:
            return 0;
    }
    return 0;
}

inline ChecksumType GetTypeFromString(const std::string &str) {
    if (str == "crc32c") {
        return ChecksumType::kCRC32C;
    } else if (str == "md5") {
        return ChecksumType::kMD5;
    } else if (str == "sha1") {
        return ChecksumType::kSHA1;
    } else if (str == "sha256") {
        return ChecksumType::kSHA256;
    }
    return ChecksumType::kUnknown;
}

// A single checksum type / value pair
// Value is stored as raw bytes in memory.
struct ChecksumEntry {
    ChecksumType type{ChecksumType::kUnknown};
    std::array<unsigned char, g_max_checksum_length> value;
};

// All known checksums for a given object
//
// If the checksum is not available, then checksums[ctype].type == kUnknown.
// Otherwise, checksums[type].value contains the checksum and checksums[ctype].type
// is set to ctype for a given ctype in the ChecksumType enum.
class ChecksumInfo {
public:

    // Check to see if a checksum's value is set
    //
    // Always returns false if kAll or kUnknown are requested.
    bool IsSet(ChecksumType ctype) const {
        if ((ctype == ChecksumType::kUnknown) || (ctype == ChecksumType::kAll)) return false;
        return checksums[static_cast<size_t>(ctype)].type != ChecksumType::kUnknown;
    }

    // Get the checksum value for a given checksum type
    //
    // If an invalid value is requested (kAll, kUnknown), then the return value is
    // undefined (as opposed to throwing an exception).
    //
    // If the checksum value is not set (IsSet returns false), then the return value
    // is undefined.
    const std::array<unsigned char, g_max_checksum_length> &Get(ChecksumType ctype) const {
        if ((ctype == ChecksumType::kUnknown) || (ctype == ChecksumType::kAll))
            ctype = ChecksumType::kCRC32C;
        return checksums[static_cast<size_t>(ctype)].value;
    }

    // Set the value of the checksum for a known type.
    //
    // Returns true if the checksum type is valid, false otherwise (kAll, kUnknown).
    bool Set(ChecksumType ctype, const std::array<unsigned char, g_max_checksum_length> &value) {
        if ((ctype == ChecksumType::kUnknown) || (ctype == ChecksumType::kAll)) return false;
        checksums[static_cast<size_t>(ctype)] = ChecksumEntry{ctype, value};
        return true;
    }

    // Returns a tuple of the type and value of the first set checksum in the object.
    //
    // If no value is set, then the returned type will be kUnknown.
    std::tuple<ChecksumType, std::array<unsigned char, g_max_checksum_length>, bool> GetFirst() const {
        for (int idx=0; idx < static_cast<int>(ChecksumType::kAll); ++idx) {
            if (checksums[idx].type != ChecksumType::kUnknown) {
                return std::make_tuple(static_cast<ChecksumType>(idx), checksums[idx].value, true);
            }
        }
        return std::make_tuple(ChecksumType::kUnknown, std::array<unsigned char, g_max_checksum_length>(), false);
    }

private:
    std::array<ChecksumEntry, static_cast<int>(ChecksumType::kAll)> checksums;
};

}

#endif // XRDCLCURLCHECKSUM_HH_

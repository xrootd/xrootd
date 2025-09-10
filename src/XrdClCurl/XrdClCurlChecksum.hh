/***************************************************************
 *
 * Copyright (C) 2025, Pelican Project, Morgridge Institute for Research
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

#ifndef XRDCLCURLCHECKSUM_HH_
#define XRDCLCURLCHECKSUM_HH_

#include <array>
#include <string>
#include <tuple>

namespace XrdClCurl {

constexpr size_t g_max_checksum_length = 32;

// Checksum types known to this cache
enum class ChecksumType {
    kCRC32C = 0,
    kMD5 = 1,
    kSHA1 = 2,
    kSHA256 = 3,
    kUnknown = 4,
    kAll = 4
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

class ChecksumTypeBitmask {
public:
    void Set(ChecksumType ctype) {
        m_mask |= 1 << static_cast<int>(ctype);
    }
    void Clear(ChecksumType ctype) {
        m_mask &= ~(1 << static_cast<int>(ctype));
    }
    bool Test(ChecksumType ctype) const {
        return m_mask & (1 << static_cast<int>(ctype));
    }
    bool TestAny() const {
        return m_mask != 0;
    }
    void SetAll() {
        m_mask = (1 << static_cast<int>(ChecksumType::kAll)) - 1;
    }
    void ClearAll() {
        m_mask = 0;
    }
    unsigned Count() const {
        unsigned count = 0;
        for (int idx=0; idx < static_cast<int>(ChecksumType::kAll); ++idx) {
            if (m_mask & (1 << idx)) {
                ++count;
            }
        }
        return count;
    }
    unsigned Get() const {
        return m_mask;
    }
    ChecksumType GetFirst() const {
        for (int idx=0; idx < static_cast<int>(ChecksumType::kAll); ++idx) {
            if (m_mask & (1 << idx)) {
                return static_cast<ChecksumType>(idx);
            }
        }
        return ChecksumType::kUnknown;
    }
private:
    char m_mask{0}; 
};

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
    bool IsSet(ChecksumType ctype) const {
        return checksums[static_cast<size_t>(ctype)].type != ChecksumType::kUnknown;
    }
    const std::array<unsigned char, g_max_checksum_length> &Get(ChecksumType ctype) const {
        return checksums[static_cast<size_t>(ctype)].value;
    }
    void Set(ChecksumType ctype, const std::array<unsigned char, g_max_checksum_length> &value) {
        checksums[static_cast<size_t>(ctype)] = ChecksumEntry{ctype, value};
    }

    std::tuple<ChecksumType, std::array<unsigned char, g_max_checksum_length>, bool> GetFirst() const {
        for (int idx=0; idx < static_cast<int>(ChecksumType::kUnknown); ++idx) {
            if (checksums[idx].type != ChecksumType::kUnknown) {
                return std::make_tuple(static_cast<ChecksumType>(idx), checksums[idx].value, true);
            }
        }
        return std::make_tuple(ChecksumType::kUnknown, std::array<unsigned char, g_max_checksum_length>(), false);
    }

private:
    std::array<ChecksumEntry, static_cast<int>(ChecksumType::kUnknown)> checksums;
};

}

#endif // XRDCLCURLCHECKSUM_HH_

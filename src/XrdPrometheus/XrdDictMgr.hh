
#pragma once

#include <ctime>
#include <string>
#include <unordered_map>

/**
 * Maintain a mapping from a dictionary ID from a XRootD monitoring stream
 * to a user record structure
 */
template<class RecordT> class DictIDManager {
public:
    using dictid_type = typename RecordT::id_t;
    using key_type = dictid_type;
    using value_type = std::pair<dictid_type, RecordT>;
    using difference_type = std::ptrdiff_t;

    time_t default_lifetime = 86400;

private:
    using TimedRecord = std::pair<time_t, RecordT>;
    using DictMap = std::unordered_map<dictid_type, TimedRecord>;

public:

    /**
     * Iterate through the managed objects, expiring as necessary.
     */
    void
    purge()
    {
        auto now = time(NULL);
        // TODO: Once C++20 is supported, replace this with std::erase_if.
        for (auto iter = m_map.begin(), last = m_map.end(); iter != last; ) {
            if (iter->second.first < now) {
                iter = m_map.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    class iterator {
        friend class DictIDManager;

        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = std::pair<dictid_type, RecordT>;
            using pointer = value_type*;
            using reference = value_type&;

            iterator(typename DictMap::iterator itr) :
                m_itr(itr)
            {}

            reference operator*() const {
                if (!m_made) {m_cur.first = m_itr->first; m_cur.second = m_itr->second.second;}
                m_made = true;
                return m_cur;
            }
            pointer operator->() {
                if (!m_made) {
                    m_cur.first = m_itr->first;
                    m_cur.second = m_itr->second.second;
                }
                m_made = true;
                return &m_cur;
            }
            iterator& operator++()
            {
                m_itr++;
                m_made = false;
            }
            iterator operator++(int) {iterator tmp = *this; ++(*this); return tmp;}

            friend bool operator== (const iterator& left, const iterator& right) {return left.m_itr == right.m_itr;}
            friend bool operator!= (const iterator& left, const iterator& right) {return left.m_itr != right.m_itr;}

            /**
             * Update the expiration time for a record
             */
            void update_expiry(time_t new_expiry) {m_itr->second.first = new_expiry;}

        private:
            mutable bool m_made{false};
            mutable value_type m_cur;
            typename DictMap::iterator m_itr;
    };

    iterator find(const dictid_type &id) {
        auto itr = m_map.find(id);
        return iterator(itr);
    }

    iterator erase(const iterator &iter)
    {
        return iterator(m_map.erase(iter.m_itr));
    }

    size_t erase(const key_type &k)
    {
        return m_map.erase(k);
    }

    iterator begin() {return m_map.begin();}
    iterator end() {return m_map.end();}

    // TODO: when C++17 is supported, add move semantics
    std::pair<iterator, bool>
    insert(const value_type& value)
    {
        typename DictMap::value_type new_value{value.first, {time(NULL) + default_lifetime, value.second}};
        auto result = m_map.insert(new_value);
        return {iterator(result.first), result.second};
    }

private:
    DictMap m_map;
};


/**
 * Note on monitoring IDs:
 * - dictid: Transient identifier for a message.  Can be many different things.
 *       Do not use as this is ambiguous.  32-bit.
 * - uid (user id): the dict id on the user login map message.  Used by the f-stream.
 * - sid (session id): session ID generated each time a user logs in.  Presented as
         ASCII; unclear the size so 32-bit is used.
 * - server id: Unused.  Generated to disambiguate messages between multiple servers.
 *       Store as 64-bits; masked to 48-bits.
 *
 * Notice the documentation refers to the sid and server id as `sid`.
 */


struct UserRecord {
public:
    using id_t = uint32_t; // this is the uid.
    using sid_t = uint32_t; // this is the sid.
    std::string authenticationProtocol;
    std::string dn;
    std::string role;
    std::string org;
};

struct FileRecord {
public:
    using id_t = uint32_t;
    UserRecord::id_t userid;
    std::string path;
    uint32_t read_ops{0};
    uint32_t readv_ops{0};
    uint32_t write_ops{0};
    uint64_t readv_segs{0};
    uint64_t read_bytes{0};
    uint64_t readv_bytes{0};
    uint64_t write_bytes{0};
};

using UserRecordManager = DictIDManager<UserRecord>;
using FileRecordManager = DictIDManager<FileRecord>;

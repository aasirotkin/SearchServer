#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
    struct ConcurrentMapStruct {
        std::mutex mute;
        std::map<Key, Value> concurrent_map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : maps_(bucket_count) {
    }

    Access operator[](const Key& key) {
        ConcurrentMapStruct& elem = maps_.at(static_cast<uint64_t>(key) % maps_.size());
        return { std::lock_guard<std::mutex>(elem.mute), elem.concurrent_map[key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> full_map;
        for (auto& [mute, local_map] : maps_) {
            std::lock_guard<std::mutex> guard(mute);
            full_map.insert(local_map.begin(), local_map.end());
        }
        return full_map;
    }

    void erase(const Key& key) {
        ConcurrentMapStruct& elem = maps_.at(static_cast<uint64_t>(key) % maps_.size());
        std::lock_guard<std::mutex> guard(elem.mute);
        elem.concurrent_map.erase(key);
    }

private:
    std::vector<ConcurrentMapStruct> maps_;
};

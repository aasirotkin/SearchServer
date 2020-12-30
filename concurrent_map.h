#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        Value& ref_to_value;
        std::mutex& mute;

        Access(Value& ref, std::mutex& mute)
            : ref_to_value(ref)
            , mute(mute) {
        }

        ~Access() {
            mute.unlock();
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : maps_(bucket_count)
        , mutexs_(bucket_count) {

    }

    Access operator[](const Key& key) {
        size_t map_index = MapIndex(key);
        mutexs_.at(map_index).lock();
        if (maps_.at(map_index).count(key) == 0) {
            maps_.at(map_index)[key] = Value();
        }
        return Access(maps_.at(map_index)[key], mutexs_.at(map_index));
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> full_map;
        int map_index = 0;
        for (const std::map<Key, Value>& local_map : maps_) {
            std::lock_guard<std::mutex> guard(mutexs_.at(map_index));
            full_map.insert(local_map.begin(), local_map.end());
            map_index++;
        }
        return full_map;
    }

    void erase(const Key& key) {
        size_t map_index = MapIndex(key);
        maps_.at(map_index).erase(key);
    }

private:
    size_t MapIndex(const Key& key) {
        uint64_t local_key = static_cast<uint64_t>(key);
        return (local_key % maps_.size());
    }

private:
    std::vector<std::map<Key, Value>> maps_;
    std::vector<std::mutex> mutexs_;
};
#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "KLfuCache.h"

namespace KamaCache {

template<typename Key, typename Value>
class KHashLfuCache {
public:
    KHashLfuCache(size_t capacity, int sliceNum = 0, int maxAverageNum = 10)
        : sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
        , capacity_(capacity) {
        size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_));
        for (int i = 0; i < sliceNum_; ++i) {
            lfuSliceCaches_.emplace_back(std::make_unique<KLfuCache<Key, Value>>(sliceSize, maxAverageNum));
        }
    }

    void put(const Key& key, const Value& value) {
        size_t index = hashKey(key);
        lfuSliceCaches_[index]->put(key, value);
    }

    bool get(const Key& key, Value& value) {
        size_t index = hashKey(key);
        return lfuSliceCaches_[index]->get(key, value);
    }

    Value get(const Key& key) {
        Value val;
        if (!get(key, val)) throw std::runtime_error("Key not found");
        return val;
    }

    void purge() {
        for (auto& lfu : lfuSliceCaches_) {
            lfu->purge();
        }
    }

private:
    size_t hashKey(const Key& key) const {
        std::hash<Key> hasher;
        return hasher(key) % sliceNum_;
    }

    size_t capacity_;
    int sliceNum_;
    std::vector<std::unique_ptr<KLfuCache<Key, Value>>> lfuSliceCaches_;
};

} // namespace KamaCache
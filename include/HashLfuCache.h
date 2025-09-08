#pragma once

#include "CachePolicy.h"
#include "LfuCache.h"
#include <vector>
#include <memory>
#include <thread>
#include <cmath>
#include <functional>

namespace CacheSystem {

template<typename Key, typename Value>
class HashLfuCache : public CachePolicy<Key, Value> {
public:
    HashLfuCache(size_t totalCapacity, int sliceNum = std::thread::hardware_concurrency())
        : sliceNum_(sliceNum > 0 ? sliceNum : 1)
        , capacity_(totalCapacity) {
        size_t sliceCap = std::ceil(totalCapacity / static_cast<double>(sliceNum_));
        for (int i = 0; i < sliceNum_; i++) {
            shards_.emplace_back(std::make_unique<LfuCache<Key, Value>>(sliceCap));
        }
    }

    void put(Key key, Value value) override {
        getShard(key)->put(key, value);
    }

    bool get(Key key, Value& value) override {
        return getShard(key)->get(key, value);
    }

    Value get(Key key) override {
        return getShard(key)->get(key);
    }

    void purge() {
        for (auto& shard : shards_) {
            shard->purge();
        }
    }

private:
    CachePolicy<Key, Value>* getShard(const Key& key) {
        size_t idx = std::hash<Key>{}(key) % sliceNum_;
        return shards_[idx].get();
    }

private:
    int sliceNum_;
    size_t capacity_;
    std::vector<std::unique_ptr<LfuCache<Key, Value>>> shards_;
};

} // namespace CacheSystem